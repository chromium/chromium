// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/certificate_provider/test_certificate_provider_extension_mixin.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/simple_feature.h"

namespace ash {
namespace {

constexpr char kUserEmail[] = "testuser@example.com";

Profile* GetProfile() {
  return ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

extensions::ProcessManager* GetProcessManager() {
  return extensions::ProcessManager::Get(GetProfile());
}

}  // namespace

class ChallengeResponseAuthKeysLoaderBrowserTest : public OobeBaseTest {
 public:
  ChallengeResponseAuthKeysLoaderBrowserTest() {
    // Allow the forced installation of extensions in the background.
    needs_background_networking_ = true;
  }
  ChallengeResponseAuthKeysLoaderBrowserTest(
      const ChallengeResponseAuthKeysLoaderBrowserTest&) = delete;
  ChallengeResponseAuthKeysLoaderBrowserTest& operator=(
      const ChallengeResponseAuthKeysLoaderBrowserTest&) = delete;
  ~ChallengeResponseAuthKeysLoaderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    challenge_response_auth_keys_loader_ =
        std::make_unique<ChallengeResponseAuthKeysLoader>();
    challenge_response_auth_keys_loader_->SetMaxWaitTimeForTesting(
        base::TimeDelta::Max());

    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetProfile(), &device_state_mixin_);

    // Register the ChallengeResponseKey for the user.
    user_manager::KnownUser(g_browser_process->local_state())
        .SaveKnownUser(account_id_);
  }

  void TearDownOnMainThread() override {
    if (!should_delete_loader_after_shutdown_)
      challenge_response_auth_keys_loader_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  void RegisterChallengeResponseKey(bool with_extension_id) {
    std::vector<ChallengeResponseKey> challenge_response_keys;
    ChallengeResponseKey challenge_response_key;
    challenge_response_key.set_public_key_spki_der(GetSpki());
    if (with_extension_id)
      challenge_response_key.set_extension_id(extension_id());

    challenge_response_keys.push_back(challenge_response_key);
    base::Value::List challenge_response_keys_value =
        SerializeChallengeResponseKeysForKnownUser(challenge_response_keys);
    user_manager::KnownUser(g_browser_process->local_state())
        .SetChallengeResponseKeys(account_id_,
                                  std::move(challenge_response_keys_value));
  }

  void OnAvailableKeysLoaded(
      base::RepeatingClosure run_loop_quit_closure,
      std::vector<ChallengeResponseKey>* out_challenge_response_keys,
      std::vector<ChallengeResponseKey> challenge_response_keys) {
    out_challenge_response_keys->swap(challenge_response_keys);
    run_loop_quit_closure.Run();
  }

  ChallengeResponseAuthKeysLoader::LoadAvailableKeysCallback CreateCallback(
      base::RepeatingClosure run_loop_quit_closure,
      std::vector<ChallengeResponseKey>* challenge_response_keys) {
    return base::BindOnce(
        &ChallengeResponseAuthKeysLoaderBrowserTest::OnAvailableKeysLoaded,
        weak_ptr_factory_.GetWeakPtr(), run_loop_quit_closure,
        challenge_response_keys);
  }

  void InstallExtension(bool wait_on_extension_loaded) {
    test_certificate_provider_extension_mixin_.ForceInstall(
        GetProfile(), /*wait_on_extension_loaded=*/wait_on_extension_loaded,
        /*immediately_provide_certificates=*/wait_on_extension_loaded);
  }

  std::vector<ChallengeResponseKey> LoadChallengeResponseKeys() {
    base::RunLoop run_loop;
    std::vector<ChallengeResponseKey> challenge_response_keys;
    challenge_response_auth_keys_loader_->LoadAvailableKeys(
        account_id_,
        CreateCallback(run_loop.QuitClosure(), &challenge_response_keys));
    run_loop.Run();
    return challenge_response_keys;
  }

  static std::string GetSpki() {
    return TestCertificateProviderExtension::GetCertificateSpki();
  }

  static extensions::ExtensionId extension_id() {
    return TestCertificateProviderExtension::extension_id();
  }

  AccountId account_id() const { return account_id_; }

  ChallengeResponseAuthKeysLoader* challenge_response_auth_keys_loader() {
    return challenge_response_auth_keys_loader_.get();
  }

  void DeleteChallengeResponseAuthKeysLoader() {
    challenge_response_auth_keys_loader_.reset();
  }

  void set_should_delete_loader_after_shutdown() {
    should_delete_loader_after_shutdown_ = true;
  }

 private:
  const AccountId account_id_{AccountId::FromUserEmail(kUserEmail)};

  // Bypass "signin_screen" feature only enabled for allowlisted extensions.
  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      feature_allowlist_{extension_id()};

  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  TestCertificateProviderExtensionMixin
      test_certificate_provider_extension_mixin_{
          &mixin_host_, &extension_force_install_mixin_};

  std::unique_ptr<ChallengeResponseAuthKeysLoader>
      challenge_response_auth_keys_loader_;

  // Whether `challenge_response_auth_keys_loader_` should be destroyed after
  // the browser shutdown, not before it.
  bool should_delete_loader_after_shutdown_ = false;

  base::WeakPtrFactory<ChallengeResponseAuthKeysLoaderBrowserTest>
      weak_ptr_factory_{this};
};

// Tests the error case when no key is registered for the current user.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       NoKeyRegistered) {
  InstallExtension(/*wait_on_extension_loaded=*/true);

  // Challenge Response Auth Keys cannot be loaded.
  EXPECT_FALSE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));
  EXPECT_EQ(LoadChallengeResponseKeys().size(), static_cast<size_t>(0));
}

// Tests the error case when no extension providing keys is installed.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       NoExtensions) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns no keys, since there's no extension available.
  EXPECT_EQ(LoadChallengeResponseKeys().size(), static_cast<size_t>(0));
}

// Tests that auth keys can be loaded with an extension providing them already
// in place.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       LoadingKeysAfterExtensionIsInstalled) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/true);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns the expected keys.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  ASSERT_EQ(challenge_response_keys.size(), static_cast<size_t>(1));
  EXPECT_EQ(challenge_response_keys.at(0).extension_id(), extension_id());
  EXPECT_EQ(challenge_response_keys.at(0).public_key_spki_der(), GetSpki());
}

// Tests that auth keys can be loaded while the extension providing them is is
// already registered as force-installed, but installation is not yet complete.
// ChallengeResponseAuthKeysLoader needs to wait on the installation to complete
// instead of incorrectly responding that there are no available certificates.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       LoadingKeysWhileExtensionIsBeingInstalled) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns the expected keys.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  ASSERT_EQ(challenge_response_keys.size(), static_cast<size_t>(1));
  EXPECT_EQ(challenge_response_keys.at(0).extension_id(), extension_id());
  EXPECT_EQ(challenge_response_keys.at(0).public_key_spki_der(), GetSpki());
}

// Tests running into the timeout when waiting for extensions.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       TimeoutWhileWaitingOnExtensionInstallation) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);
  challenge_response_auth_keys_loader()->SetMaxWaitTimeForTesting(
      base::TimeDelta::Min());

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns before any keys are available.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  EXPECT_EQ(challenge_response_keys.size(), static_cast<size_t>(0));
}

// Tests flow when there is no stored extension_id, for backward compatibility.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       LoadingKeysWithoutExtensionId) {
  RegisterChallengeResponseKey(/*with_extension_id=*/false);
  InstallExtension(/*wait_on_extension_loaded=*/true);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns the expected keys.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  ASSERT_EQ(challenge_response_keys.size(), static_cast<size_t>(1));
  EXPECT_EQ(challenge_response_keys.at(0).extension_id(), extension_id());
  EXPECT_EQ(challenge_response_keys.at(0).public_key_spki_der(), GetSpki());
}

// Tests the case when the loader is destroyed before the operation completes.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       DestroyedBeforeCompletion) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // Start the LoadAvailableKeys operation. The operation is expected to never
  // complete.
  challenge_response_auth_keys_loader()->LoadAvailableKeys(
      account_id(),
      base::BindOnce(
          [](std::vector<ChallengeResponseKey> challenge_response_keys) {
            ADD_FAILURE();
          }));
  // Destroy the loader immediately.
  DeleteChallengeResponseAuthKeysLoader();
}

// Tests the case when the load operation isn't completed by the time the
// browser shuts down.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       AfterShutdown) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // Start the key loading operation. Intentionally do not wait for its
  // completion.
  challenge_response_auth_keys_loader()->LoadAvailableKeys(account_id(),
                                                           base::DoNothing());
  // Postpone destroying the loader until after the browser shutdown. No crash
  // should occur.
  set_should_delete_loader_after_shutdown();
}

class ChallengeResponseExtensionLoadObserverTest
    : public ChallengeResponseAuthKeysLoaderBrowserTest,
      public extensions::ProcessManagerObserver {
 public:
  ChallengeResponseExtensionLoadObserverTest() = default;
  ChallengeResponseExtensionLoadObserverTest(
      const ChallengeResponseExtensionLoadObserverTest&) = delete;
  ChallengeResponseExtensionLoadObserverTest& operator=(
      const ChallengeResponseExtensionLoadObserverTest&) = delete;
  ~ChallengeResponseExtensionLoadObserverTest() override = default;

  void SetUpOnMainThread() override {
    ChallengeResponseAuthKeysLoaderBrowserTest::SetUpOnMainThread();
    process_manager_observation_.Observe(GetProcessManager());
  }

  void TearDownOnMainThread() override {
    process_manager_observation_.Reset();
    ChallengeResponseAuthKeysLoaderBrowserTest::TearDownOnMainThread();
  }

  void SetExtensionHostCreatedLoop(base::RunLoop* run_loop) {
    extension_host_created_loop_ = run_loop;
  }

  void StartLoadingChallengeResponseKeys(base::RunLoop* run_loop) {
    // Result should be empty and will be discarded.
    challenge_response_auth_keys_loader()->LoadAvailableKeys(
        account_id(),
        base::BindLambdaForTesting([=](std::vector<ChallengeResponseKey> keys) {
          EXPECT_TRUE(keys.empty());
          run_loop->Quit();
        }));
  }

  void WaitForExtensionHostAndDestroy() {
    extension_host_created_loop_->Run();
    // Simulate Extension Host dying unexpectedly.
    // Destroying this triggers a notification for the extension subsystem,
    // which will deregister the host.
    delete extension_host_;
    extension_host_ = nullptr;
  }

  // extensions::ProcessManagerObserver

  void OnBackgroundHostCreated(
      extensions::ExtensionHost* extension_host) override {
    if (extension_host->extension_id() == extension_id()) {
      extension_host_ = extension_host;
      extension_host_created_loop_->Quit();
    }
  }

  void OnProcessManagerShutdown(extensions::ProcessManager* manager) override {
    DCHECK(process_manager_observation_.IsObservingSource(manager));
    process_manager_observation_.Reset();
  }

 private:
  raw_ptr<base::RunLoop> extension_host_created_loop_ = nullptr;
  raw_ptr<extensions::ExtensionHost, DanglingUntriaged> extension_host_ =
      nullptr;
  base::ScopedObservation<extensions::ProcessManager,
                          extensions::ProcessManagerObserver>
      process_manager_observation_{this};
};

// Tests that observers get cleaned up properly if the observed ExtensionHost
// is destroyed earlier than the observing ExtensionLoadObserver.
IN_PROC_BROWSER_TEST_F(ChallengeResponseExtensionLoadObserverTest,
                       ExtensionHostDestroyedEarly) {
  base::RunLoop extension_host_created_loop;
  SetExtensionHostCreatedLoop(&extension_host_created_loop);

  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  base::RunLoop load_challenge_response_keys_complete;
  StartLoadingChallengeResponseKeys(&load_challenge_response_keys_complete);
  WaitForExtensionHostAndDestroy();
  load_challenge_response_keys_complete.Run();
}

}  // namespace ash
