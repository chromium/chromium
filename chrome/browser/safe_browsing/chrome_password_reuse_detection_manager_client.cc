// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/password_reuse_signal.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/scoped_allow_sync_call.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

using password_manager::metrics_util::PasswordType;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;

// Adds |observer| to the input observers of |widget_host|.
void AddToWidgetInputEventObservers(
    content::RenderWidgetHost* widget_host,
    content::RenderWidgetHost::InputEventObserver* observer) {
#if BUILDFLAG(IS_ANDROID)
  widget_host->AddImeInputEventObserver(observer);
#endif
  widget_host->AddInputEventObserver(observer);
}

#if !BUILDFLAG(IS_ANDROID)
// Retrieves and formats the saved passwords domains from signon_realms.
std::vector<std::string> GetMatchingDomains(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  base::flat_set<std::string> matching_domains;
  for (const auto& credential : matching_reused_credentials) {
    // TODO(crbug.com/40895227): Avoid converting signon_realm to URL,
    // ideally use PasswordForm::url.
    std::string domain = base::UTF16ToUTF8(url_formatter::FormatUrl(
        GURL(credential.signon_realm),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    matching_domains.insert(std::move(domain));
  }
  return std::move(matching_domains).extract();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// static
void ChromePasswordReuseDetectionManagerClient::CreateForWebContents(
    content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }

  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(
          new ChromePasswordReuseDetectionManagerClient(contents)));
}

// static
void ChromePasswordReuseDetectionManagerClient::
    CreateForProfilePickerWebContents(content::WebContents* contents) {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kSavePasswordHashFromProfilePicker)) {
    return;
  }
  if (FromWebContents(contents)) {
    return;
  }
  // ChromePasswordReuseDetectionManagerClient depends on
  // ChromePasswordManagerClient for obtaining objects it needs to attempt
  // saving password hashes. ChromePasswordManagerClient depends on
  // AutofillClientProvider.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  autofill::AutofillClientProvider& autofill_client_provider =
      autofill::AutofillClientProviderFactory::GetForProfile(profile);
  autofill_client_provider.CreateClientForWebContents(contents);
  ChromePasswordManagerClient::CreateForWebContents(contents);
  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new ChromePasswordReuseDetectionManagerClient(
          contents, IdentityManagerFactory::GetForProfile(profile))));
}

ChromePasswordReuseDetectionManagerClient::
    ~ChromePasswordReuseDetectionManagerClient() {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
}

void ChromePasswordReuseDetectionManagerClient::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  ChromePasswordManagerClient* password_manager_client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  if (password_manager_client) {
    InternalOnPrimaryAccountChanged(password_manager_client, event_details);
  }
}

void ChromePasswordReuseDetectionManagerClient::InternalOnPrimaryAccountChanged(
    password_manager::PasswordManagerClient* password_manager_client,
    const signin::PrimaryAccountChangeEvent& event_details) {
  // If a sign-in to Chrome was detected and the PasswordManager has
  // submitted credentials, we think we've encountered gaia credentials.
  // So, let's try to save a password hash for password reuse detection.
  if ((event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
       signin::PrimaryAccountChangeEvent::Type::kSet)) {
    std::optional<password_manager::PasswordForm> password_form =
        password_manager_client->GetPasswordManager()
            ->GetSubmittedCredentials();
    if (!password_form.has_value()) {
      base::UmaHistogramBoolean(
          "PasswordProtection.AttemptsToSavePasswordHashFromProfilePicker",
          false);
      return;
    }
    password_manager_client->GetPasswordReuseManager()->MaybeSavePasswordHash(
        &password_form.value(), password_manager_client);
    base::UmaHistogramBoolean(
        "PasswordProtection.AttemptsToSavePasswordHashFromProfilePicker", true);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromePasswordReuseDetectionManagerClient::OnPasswordSelected(
    const std::u16string& text) {
  password_reuse_detection_manager_.OnPaste(text);
}

void ChromePasswordReuseDetectionManagerClient::OnImeTextCommittedEvent(
    const std::u16string& text_str) {
  password_reuse_detection_manager_.OnKeyPressedCommitted(text_str);
}

void ChromePasswordReuseDetectionManagerClient::OnImeSetComposingTextEvent(
    const std::u16string& text_str) {
  last_composing_text_ = text_str;
  password_reuse_detection_manager_.OnKeyPressedUncommitted(
      last_composing_text_);
}

void ChromePasswordReuseDetectionManagerClient::
    OnImeFinishComposingTextEvent() {
  password_reuse_detection_manager_.OnKeyPressedCommitted(last_composing_text_);
  last_composing_text_.clear();
}
#endif  // BUILDFLAG(IS_ANDROID)

autofill::LogManager*
ChromePasswordReuseDetectionManagerClient::GetLogManager() {
  return log_manager_.get();
}

password_manager::PasswordReuseManager*
ChromePasswordReuseDetectionManagerClient::GetPasswordReuseManager() const {
  return PasswordReuseManagerFactory::GetForProfile(profile_);
}

const GURL& ChromePasswordReuseDetectionManagerClient::GetLastCommittedURL()
    const {
  return web_contents()->GetLastCommittedURL();
}

safe_browsing::PasswordProtectionService*
ChromePasswordReuseDetectionManagerClient::GetPasswordProtectionService()
    const {
  return safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(profile_);
}

void ChromePasswordReuseDetectionManagerClient::
    MaybeLogPasswordReuseDetectedEvent() {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (pps) {
    pps->MaybeLogPasswordReuseDetectedEvent(web_contents());
  }
}

bool ChromePasswordReuseDetectionManagerClient::IsHistorySyncAccountEmail(
    const std::string& username) {
  Profile* original_profile = profile_->GetOriginalProfile();
  // Password reuse detection is tied to history sync.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(original_profile);
  if (!sync_service || !sync_service->GetPreferredDataTypes().Has(
                           syncer::HISTORY_DELETE_DIRECTIVES)) {
    return false;
  }
  return password_manager::sync_util::IsSyncAccountEmail(
      username, IdentityManagerFactory::GetForProfile(original_profile),
      signin::ConsentLevel::kSignin);
}

bool ChromePasswordReuseDetectionManagerClient::
    IsPasswordFieldDetectedOnPage() {
  auto* password_manager_client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  // Password manager client can be a nullptr in tests.
  if (!password_manager_client) {
    return false;
  }
  return password_manager_client->GetPasswordManager()
             ? password_manager_client->GetPasswordManager()
                   ->IsPasswordFieldDetectedOnPage()
             : false;
}

void ChromePasswordReuseDetectionManagerClient::CheckProtectedPasswordEntry(
    PasswordType password_type,
    const std::string& username,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists,
    uint64_t reused_password_hash,
    const std::string& domain) {
  safe_browsing::PasswordProtectionService* pps =
      GetPasswordProtectionService();
  if (!pps) {
    return;
  }

  pps->MaybeStartProtectedPasswordEntryRequest(
      web_contents(), web_contents()->GetLastCommittedURL(), username,
      password_type, matching_reused_credentials, password_field_exists);

#if !BUILDFLAG(IS_ANDROID)
  // Converts the url_string to GURL to avoid constructing it twice.
  GURL domain_gurl(domain);
  // If the webpage is not an extension page, do nothing.
  if (!domain_gurl.SchemeIs(extensions::kExtensionScheme)) {
    return;
  }
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::kExtensionTelemetryPotentialPasswordTheft)) {
    return;
  }
  // Construct password reuse info.
  safe_browsing::PasswordReuseInfo password_reuse_info;
  password_reuse_info.matches_signin_password =
      password_type == PasswordType::PRIMARY_ACCOUNT_PASSWORD;
  password_reuse_info.matching_domains =
      GetMatchingDomains(matching_reused_credentials);
  password_reuse_info.reused_password_account_type =
      pps->GetPasswordProtectionReusedPasswordAccountType(password_type,
                                                          username);
  password_reuse_info.count = 1;
  password_reuse_info.reused_password_hash = reused_password_hash;

  // Extract the host part of an extension domain, which will be the extension
  // ID.
  std::string host = domain_gurl.host();
  auto password_reuse_signal =
      std::make_unique<safe_browsing::PasswordReuseSignal>(host,
                                                           password_reuse_info);
  telemetry_service->AddSignal(std::move(password_reuse_signal));
#endif  // !BUILDFLAG(IS_ANDROID)
}

ChromePasswordReuseDetectionManagerClient::
    ChromePasswordReuseDetectionManagerClient(
        content::WebContents* web_contents,
        signin::IdentityManager* identity_manager)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromePasswordReuseDetectionManagerClient>(
          *web_contents),
      password_reuse_detection_manager_(this),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      phishy_interaction_tracker_(
          safe_browsing::PhishyInteractionTracker(web_contents)),
      identity_manager_(identity_manager) {
  log_manager_ = autofill::LogManager::Create(
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          profile_),
      base::RepeatingClosure());
  // Expected to be non-null in prod only when instantiated from
  // CreateForProfilePickerWebContents.
  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }
}

void ChromePasswordReuseDetectionManagerClient::WebContentsDestroyed() {
  phishy_interaction_tracker_.WebContentsDestroyed();
}

void ChromePasswordReuseDetectionManagerClient::PrimaryPageChanged(
    content::Page& page) {
  // Suspends logging on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);

  password_reuse_detection_manager_.DidNavigateMainFrame(GetLastCommittedURL());

  AddToWidgetInputEventObservers(page.GetMainDocument().GetRenderWidgetHost(),
                                 this);
  phishy_interaction_tracker_.HandlePageChanged();
}

void ChromePasswordReuseDetectionManagerClient::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/40833643): In context of Phishguard, we should handle
  // input events on subframes separately, so that we can accurately report that
  // the password was reused on a subframe. Currently any password reuse for
  // this WebContents will report password reuse on the main frame URL.
  AddToWidgetInputEventObservers(render_frame_host->GetRenderWidgetHost(),
                                 this);
}

void ChromePasswordReuseDetectionManagerClient::OnPaste() {
  std::u16string text;
  bool used_crosapi_workaround = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, the ozone/wayland clipboard implementation is asynchronous by
  // default and runs a nested message loop to fake synchroncity. This in turn
  // causes crashes. See https://crbug.com/1155662 for details. In the short
  // term, we skip ozone/wayland entirely and use a synchronous crosapi to get
  // clipboard text.
  // TODO(crbug.com/40605786): This logic can be removed once all
  // clipboard APIs are async.
  auto* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<crosapi::mojom::Clipboard>()) {
    used_crosapi_workaround = true;
    std::string text_utf8;
    {
      crosapi::ScopedAllowSyncCall allow_sync_call;
      service->GetRemote<crosapi::mojom::Clipboard>()->GetCopyPasteText(
          &text_utf8);
    }
    text = base::UTF8ToUTF16(text_utf8);
  }
#endif

  if (!used_crosapi_workaround) {
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    // Given that this clipboard data read happens in the background and not
    // initiated by a user gesture, then the user shouldn't see a notification
    // if the clipboard is restricted by the rules of data leak prevention
    // policy.
    ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
        ui::EndpointType::kDefault, {.notify_if_restricted = false});
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &data_dst, &text);
  }

  password_reuse_detection_manager_.OnPaste(std::move(text));
  phishy_interaction_tracker_.HandlePasteEvent();
}

void ChromePasswordReuseDetectionManagerClient::OnInputEvent(
    const blink::WebInputEvent& event) {
  phishy_interaction_tracker_.HandleInputEvent(event);
#if BUILDFLAG(IS_ANDROID)
  // On Android, key down events are triggered if a user types in through a
  // number bar on Android keyboard. If text is typed in through other parts of
  // Android keyboard, ImeTextCommittedEvent is triggered instead.
  if (event.GetType() != blink::WebInputEvent::Type::kKeyDown) {
    return;
  }
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  password_reuse_detection_manager_.OnKeyPressedCommitted(
      key_event.text.data());

#else   // !BUILDFLAG(IS_ANDROID)
  if (event.GetType() != blink::WebInputEvent::Type::kChar) {
    return;
  }
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  // Key & 0x1f corresponds to the value of the key when either the control or
  // command key is pressed. This detects CTRL+V, COMMAND+V, and CTRL+SHIFT+V.
  if (key_event.windows_key_code == (ui::VKEY_V & 0x1f)) {
    OnPaste();
  } else {
    password_reuse_detection_manager_.OnKeyPressedCommitted(
        key_event.text.data());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePasswordReuseDetectionManagerClient);
