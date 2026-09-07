// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/signin_profile_handler.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/signin_partition_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/crx_file/id_util.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

// This set contains a subset of the explicitly allowlisted extensions that
// are defined in extensions/common/api/_behavior_features.json. The extension
// is treated as risky if it has some UI elements which remain accessible
// after the signin was completed.
constexpr auto kNonRiskyExtensionsIdsHashes =
    base::MakeFixedFlatSet<std::string_view>({
        // Gnubby component extension (kmendfapggjehodndflmmgagdbamhnfd)
        "FCE3552DB7971D9A2003B5BAE26D4B156A7308E04A997F1503C9BF54DF5A33DB",
        // Gnubby app (beknehfpfkghjoafdifaflglpjkojoco)
        "78578251B1309A5E256FD8CC60C538C958FAA1BC285486236A9D75A0994F0AF4",
        // Chrome OS XKB (jkghodnilhceideoidjikpgommlajknk)
        "61823797C4EEBFFADEA03397E8FB61F09092F10D3223A2BDC7428159C47F0199",
        // Virtual Keyboard (mppnpdlheglhdfmldimlhpnegondlapf)
        "94258E7037909EA26C3BB6F0183A0BD126469795D5BB70DF2E4F2D725FDBD4F3",
        // Braille Keyboard (jddehjeebkoimngcbdkaahpobgicbffp)
        "8BA42F8465689319BC18E81D3F0029ABEF8EE162147B8740AC206AB3AB140811",
        // Speech synthesis (gjjabgpgjpampikjhjpfhneeoapjbjaf)
        "E0369D3619CEABC15BFEA6E33F85637C26E1E13D5CE45B83E8A7E963008E556D",
        // Mobile activation (iadeocfgjdjdmpenejdbfeaocpbikmab)
        "1C1514FFE21EDD8FEADF0A5F0CABBCDD94C36A58CB085D8A17AE069BEFDB88FE",
        // Chromeos help (honijodknafkokifofgiaalefdiedpko)
        "7C852E22253B325BDEEEF1E00343D301F0620FCFC324919236A44C554F8AAC4B",
        // Easy unlock (mkaemigholebcgchlkbankmihknojeak)
        "1BAD9E99D2B8A183AD981DCDD68F297E54D4A743AAC395BCFAFE8A471BE328E9",
        // ChromeVox (mndnfokpggljbaajbnioimlmbfngpief)
        "5C436709A74404D3816118160F896544B352CF1696FB387B4FC37258AD75C4B2",
        // Switch Access (pmehocpgjmkenlokgjfkaichfjdhpeol)
        "73EBB60B47BC6569FC79E400EA5E693DD852C5A5B859FF7DD4E9B59C4D13812D",
        // Select-to-speak (klbcgckkldhdhonijdbnhhaiedfkllef)
        "3189CC596B1BA8F9987AA62F1F2C5E31BC06C2F27CD4A0A4719C8517C600D736",
        // Enhanced Network TTS (jacnkoglebceckolkoapelihnglgaicd)
        "24578E07F497551DE0F76DC1766B210989E710341981EB717CE77F34C05D617E",
        // Accessibility Common (egfdjlfmgnehecnclamagfafdccgfndp)
        "0631CFBB5FF0C5C3C62B6E82C8C0C86B391B031FE482106338E94A68E160CCAA",
    });

void WrapAsBrowsersCloseCallback(const base::RepeatingClosure& callback,
                                 const base::FilePath& path) {
  callback.Run();
}

SigninProfileHandler* g_instance = nullptr;

}  // namespace

SigninProfileHandler::SigninProfileHandler() {
  DCHECK(!g_instance);
  g_instance = this;
}

SigninProfileHandler::~SigninProfileHandler() {
  if (browsing_data_remover_)
    browsing_data_remover_->RemoveObserver(this);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

SigninProfileHandler* SigninProfileHandler::Get() {
  return g_instance;
}

void SigninProfileHandler::ProfileStartUp(Profile* profile) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the off-the-record profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetPrimaryOTRProfile() call above.
  profile->InitChromeOSPreferences();

  // Add observer so we can see when the first profile's session restore is
  // completed. After that, we won't need the default profile anymore.
  if (!ash::IsSigninBrowserContext(profile) &&
      user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount() &&
      !user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    auto* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
    if (login_manager)
      login_manager->AddObserver(this);
  }
}

void SigninProfileHandler::ClearSigninProfile(base::OnceClosure callback) {
  on_clear_callbacks_.push_back(std::move(callback));

  // Profile is already clearing.
  if (on_clear_callbacks_.size() > 1)
    return;

  auto* signin_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetSigninBrowserContext());
  if (!signin_profile) {
    OnSigninProfileCleared();
    return;
  }

  on_clear_profile_stage_finished_ = base::BarrierClosure(
      3, base::BindOnce(&SigninProfileHandler::OnSigninProfileCleared,
                        weak_factory_.GetWeakPtr()));
  DCHECK(!browsing_data_remover_);
  browsing_data_remover_ = signin_profile->GetBrowsingDataRemover();
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES, this);

  // Close the current session with SigninPartitionManager. This clears cached
  // data from the last-used sign-in StoragePartition.
  login::SigninPartitionManagerFactory::GetForBrowserContext(signin_profile)
      ->CloseCurrentSigninSession(on_clear_profile_stage_finished_);

  chrome::CloseAllBrowsersWithProfile(
      signin_profile, true /* skip_beforeunload */,
      base::BindRepeating(
          &WrapAsBrowsersCloseCallback,
          on_clear_profile_stage_finished_) /* on_close_success */,
      base::BindRepeating(
          &WrapAsBrowsersCloseCallback,
          on_clear_profile_stage_finished_) /* on_close_aborted */);

  // Unload all extensions that could possibly leak the SigninProfile for
  // unauthorized usage.
  // TODO(crbug.com/40116250): This also can be fixed by restricting URLs
  //                                  or browser windows from opening.
  auto* component_loader = extensions::ComponentLoader::Get(signin_profile);
  for (const auto& el :
       component_loader->GetRegisteredComponentExtensionsIds()) {
    if (!kNonRiskyExtensionsIdsHashes.contains(
            crx_file::id_util::HashedIdInHexSha256(el))) {
      component_loader->Remove(el);
    }
  }
}

void SigninProfileHandler::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  if (state == OAuth2LoginManager::SESSION_RESTORE_DONE ||
      state == OAuth2LoginManager::SESSION_RESTORE_FAILED ||
      state == OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED) {
    auto* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    login_manager->RemoveObserver(this);
    ClearSigninProfile(base::OnceClosure());
  }
}

void SigninProfileHandler::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  DCHECK(browsing_data_remover_);
  browsing_data_remover_->RemoveObserver(this);
  browsing_data_remover_ = nullptr;

  on_clear_profile_stage_finished_.Run();
}

void SigninProfileHandler::OnSigninProfileCleared() {
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(on_clear_callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run();
  }
}

}  // namespace ash
