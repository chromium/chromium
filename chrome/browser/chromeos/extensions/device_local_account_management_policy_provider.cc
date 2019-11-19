// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <stddef.h>

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/device_local_account_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

namespace emk = extensions::manifest_keys;

// List of manifest entries from https://developer.chrome.com/apps/manifest.
// Unsafe entries are commented out and special cases too.
const char* const kSafeManifestEntries[] = {
    emk::kAboutPage,

    // Special-cased in IsSafeForPublicSession().
    // emk::kApp,

    // Not a real manifest entry (doesn't show up in code search). All legacy
    // ARC apps have this dictionary (data is stuffed there to be consumed by
    // the ARC runtime).
    "arc_metadata",

    // Documented in https://developer.chrome.com/extensions/manifest but not
    // implemented anywhere.  Still, a lot of apps use it.
    "author",

    // Allows inspection of page contents, not enabled on stable anyways except
    // for whitelist.
    // emk::kAutomation,

    "background",

    emk::kBackgroundPersistent,

    emk::kBluetooth,

    emk::kBrowserAction,

    // Allows to replace the search provider which is somewhat risky - need to
    // double check how the search provider policy behaves in PS.
    // emk::kSettingsOverride,

    // Custom bookmark managers - I think this is fair game, bookmarks should be
    // URLs only, and it's restricted to whitelist on stable.
    emk::kUIOverride,

    // Bookmark manager, history, new tab - should be safe.
    emk::kChromeURLOverrides,

    // General risk of capturing user input, but key combos must include Ctrl or
    // Alt, so I think this is safe.
    emk::kCommands,

    // General risk of capturing user input, but key combos must include Ctrl or
    // Alt, so I think this is safe.
    emk::kContentCapabilities,

    // Access to web content.
    // emk::kContentScripts,

    emk::kContentSecurityPolicy,

    // Access to web content.
    // emk::kConvertedFromUserScript,

    // An implementation detail (actually written by Chrome, not the app
    // author).
    emk::kCurrentLocale,

    // Name of directory containg default strings.
    emk::kDefaultLocale,

    // Just a display string.
    emk::kDescription,

    // Access to web content.
    // emk::kDevToolsPage,

    // Restricted to whitelist already.
    emk::kDisplayInLauncher,

    emk::kDisplayInNewTabPage,

    // This allows to declaratively filter web requests and content, matching on
    // content data. Doesn't allow direct access to request/content data. Can be
    // used to brute-force e.g. cookies (reload with filter rules adjusted to
    // match all possible cookie values) - but that's equivalent to an
    // off-device brute-force attack.
    // Looks safe in general with one exception: There's an action that allows
    // to insert content scripts on matching content. We can't allow this, need
    // to check whether there's also a host permission required for this case.
    // emk::kEventRules,

    // Shared Modules configuration: Allow other extensions to access resources.
    emk::kExport,

    emk::kExternallyConnectable,

    emk::kFileBrowserHandlers,

    // Extension file handlers are restricted to whitelist which only contains
    // quickoffice.
    emk::kFileHandlers,

    emk::kFileSystemProviderCapabilities,

    emk::kHomepageURL,

    // Just UX.
    emk::kIcons,

    // Shared Modules configuration: Import resources from another extension.
    emk::kImport,

    emk::kIncognito,

    // Keylogging.
    // emk::kInputComponents,

    // Shared Modules configuration: Specify extension id for development.
    emk::kKey,

    emk::kKiosk,

    emk::kKioskEnabled,

    // Not useful since it will prevent app from running, but we don't care.
    emk::kKioskOnly,

    emk::kKioskRequiredPlatformVersion,

    // Not useful since it will prevent app from running, but we don't care.
    emk::kKioskSecondaryApps,

    // Special-cased in IsSafeForPublicSession().
    // emk::kManifestVersion,

    emk::kMIMETypes,

    // Whitelisted to only allow browser tests and PDF viewer.
    emk::kMimeTypesHandler,

    emk::kMinimumChromeVersion,

    // NaCl modules are bound to app permissions just like the rest of the app
    // and thus should not pose a risk.
    emk::kNaClModules,

    // Just a display string.
    emk::kName,

    // Used in conjunction with the identity API - not really used when there's
    // no GAIA user signed in.
    emk::kOAuth2,

    // Generally safe (i.e. only whitelist apps), except for the policy to
    // whitelist apps for auto-approved token minting (we should just ignore
    // this in public sessions). Risk is that admin mints OAuth tokens to access
    // services on behalf of the user silently.
    // emk::kOAuth2AutoApprove,

    emk::kOfflineEnabled,

    // A bit risky as the extensions sees all keystrokes entered into the
    // omnibox after the search key matches, but generally we deem URLs fair
    // game.
    emk::kOmnibox,

    // Special-cased in IsSafeForPublicSession(). Subject to permission
    // restrictions.
    // emk::kOptionalPermissions,

    emk::kOptionsPage,

    emk::kOptionsUI,

    emk::kPageAction,

    // Special-cased in IsSafeForPublicSession(). Subject to permission
    // restrictions.
    // emk::kPermissions,

    // No constant in manifest_constants.cc. Declared as a feature, but unused.
    // "platforms",

    // Deprecated manifest entry, so we don't care.
    "plugins",

    // Stated 3D/WebGL requirements of an app.
    emk::kRequirements,

    // Execute some pages in a separate sandbox.  (Note: Using string literal
    // since extensions::manifest_keys only has constants for sub-keys.)
    "sandbox",

    // Just a display string.
    emk::kShortName,

    // Doc missing. Declared as a feature, but unused.
    // emk::kSignature,

    // Network access.
    emk::kSockets,

    // Just provides dictionaries, no access to content.
    emk::kSpellcheck,

    // (Note: Using string literal since extensions::manifest_keys only has
    // constants for sub-keys.)
    "storage",

    // Only Hangouts is whitelisted.
    emk::kSystemIndicator,

    emk::kTheme,

    // Might need this for accessibility, but has content access. Manual
    // whitelisting might be reasonable here?
    // emk::kTtsEngine,

    // TODO(tnagel): Ensure that extension updates query UserMayLoad().
    // https://crbug.com/549720
    emk::kUpdateURL,

    // Apps may intercept navigations to URL patterns for domains for which the
    // app author has proven ownership of to the Web Store.  (Chrome starts the
    // app instead of fulfilling the navigation.)  This is only safe for apps
    // that have been loaded from the Web Store and thus is special-cased in
    // IsSafeForPublicSession().
    // emk::kUrlHandlers,

    emk::kUsbPrinters,

    // Version string (for app updates).
    emk::kVersion,

    // Just a display string.
    emk::kVersionName,

    emk::kWebAccessibleResources,

    // Webview has no special privileges or capabilities.
    emk::kWebview,
};

// List of permission strings based on [1] and [2].  See |kSafePermissionDicts|
// for permission dicts.  Since Public Session users may be fully unaware of any
// apps being installed, their consent to access any kind of sensitive
// information cannot be assumed.  Therefore only APIs are whitelisted which
// should not leak sensitive data to the caller.  Since the privacy boundary is
// drawn at the API level, no safeguards are required to prevent exfiltration
// and thus apps may communicate freely over any kind of network.
// [1] https://developer.chrome.com/apps/declare_permissions
// [2] https://developer.chrome.com/apps/api_other
const char* const kSafePermissionStrings[] = {
    // Modifying accessibility settings seems safe (at most a user could be
    // confused by it).
    "accessibilityFeatures.modify",

    // Originally blocked due to concerns about leaking user health information,
    // but it seems this does more harm than good as it would likely prevent the
    // extension from enabling assistive features. If the concerns prevail, we
    // should probably not block, but adjust the API to pretend accessibility is
    // off, so we don't punish apps that try to be helpful.
    "accessibilityFeatures.read",

    // Allows access to web contents in response to user gesture. Note that this
    // doesn't trigger a permission warning on install though, so blocking is
    // somewhat at odds with the spirit of the API - however I presume the API
    // design assumes user-installed extensions, which we don't have here.
    // Whitelisted because it's restricted now (asks user for permission the
    // first time an extension tries to use it).
    "activeTab",

    // Schedule code to run at future times.
    "alarms",

    // PS UX can always be seen, this one doesn't go over it so it's fine.
    "app.window.alwaysOnTop",
    "alwaysOnTopWindows",

    // Fullscreen is crippled in Public Sessions, maximizes instead, so both
    // fullscreen and overrideEsc are safe for use in PS. (The recommended
    // permission names are "app.window.*" but their unprefixed counterparts are
    // still supported.)
    "app.window.fullscreen",
    "app.window.fullscreen.overrideEsc",
    "fullscreen",
    "overrideEscFullscreen",

    "app.window.shape",

    // The embedded app is subject to the restrictions as well obviously.
    "appview",

    // Risk of listening attack.
    // "audio",

    // User is prompted (allow/deny) when an extension requests audioCapture.
    // The request is done via the getUserMedia API.
    "audioCapture",

    // Just resource management, probably doesn't even apply to Chrome OS.
    "background",

    // Access to URLs only, no content.
    "bookmarks",

    // Open a new tab with a given URL.
    "browser",

    // This allows to read the current browsing data removal dialog settings,
    // but I don't see why this would be problematic.
    "browsingData",

    "certificateProvider",

    // clipboardRead is restricted to return an empty string (except for
    // whitelisted extensions - ie. Chrome RDP client).
    "clipboardRead",

    // Writing to clipboard is safe.
    "clipboardWrite",

    "contentSettings",

    // Privacy sensitive URL access.
    // "contextMenus",

    // This would provie access to auth cookies, so needs to be blocked.
    // "cookies",

    // Provides access to the DOM, so block.
    // "debugger",

    // This is mostly fine, but has a RequestContentScript action that'd allow
    // access to page content, which we can't allow.
    // "declarativeContent",

    // User is prompted when an extension requests desktopCapture whether they
    // want to allow it. The request is made through
    // chrome.desktopCapture.chooseDesktopMedia call.
    "desktopCapture",

    // Haven't checked in detail what this does, but messing with devtools
    // usually comes with the ability to access page content.
    // "devtools",

    // I think it's fine to allow this as it should be obvious to users that
    // scanning a document on the scanner will make it available to the
    // organization (placing a document in the scanner implies user consent).
    "documentScan",

    // Doesn't allow access to file contents AFAICT, so should be fine.
    "downloads",

    // Triggers a file open for the download.
    "downloads.open",

    // Controls shelf visibility.
    "downloads.shelf",

    "enterprise.deviceAttributes",

    "enterprise.platformKeys",

    // Possibly risky due to its experimental nature: not vetted for security,
    // potentially buggy, subject to change without notice (shouldn't
    // blanket-allow experimental stuff).
    // "experimental",

    "fileBrowserHandler",

    // Allow: (1) session state is ephemeral anyways, so no leaks across users.
    // (2) a user that stores data on an org-owned machine won't be surprised if
    // the org can see it.
    "fileSystem",

    "fileSystem.directory",

    "fileSystem.requestFileSystem",

    "fileSystem.retainEntries",

    "fileSystem.write",

    "fileSystemProvider",

    "fontSettings",

    // Just another type of connectivity.  On the system side, no user data is
    // involved, implicitly or explicity.
    "gcm",

    // It's fair game for a kiosk device owner to locate their device. Could
    // just as well do this via IP-geolocation mechanism, so little difference.
    "geolocation",

    // Somewhat risky as this opens up the ability to intercept user input.
    // However, keyboards and mice are apparently not surfaced via this API.
    "hid",

    // Privacy sensitive URL access.
    // "history",

    // Not really useful as there's no signed-in user, so OK to allow.
    "identity",

    "identity.email",

    // Detection of idle state.
    "idle",

    // IME extensions see keystrokes. This might be useful though, might rely on
    // manual whitelisting (assuming the number of useful IME extensions is
    // relatively limited).
    // "input",

    // Fair game - admin can manipulate extensions via policy anyways.
    "management",

    // Just another type of connectivity.
    "mdns",

    // Storage is ephemeral, so user needs to get their content onto the Kiosk
    // device (download or plug in media), both of which seem sufficient consent
    // actions.
    "mediaGalleries",

    "mediaGalleries.allAutoDetected",

    "mediaGalleries.copyTo",

    "mediaGalleries.delete",

    "mediaGalleries.read",

    // Probably doesn't work on Chrome OS anyways.
    "nativeMessaging",

    // Admin controls network connectivity anyways.
    "networking.config",

    // Status quo considers this risky due to the ability to fake system UI -
    // low risk IMHO however since notifications are already badged with app
    // icon and won't extract any data.
    "notifications",

    // User is prompted (allow/deny) when an extension requests pageCapture for
    // the first time in a session. The request is made via
    // chrome.pageCapture.saveAsMHTML call.
    "pageCapture",

    // Allows to use machine crypto keys - these would be provisioned by the
    // admin anyways.
    "platformKeys",

    // No plugins on Chrome OS anyways.
    "plugin",

    // Status quo notes concern about UX spoofing - not an issue IMHO.
    "pointerLock",

    // Potentiall risky: chrome.power.requestKeepAwake can inhibit idle time
    // detection and prevent idle time logout and that way reduce isolation
    // between subsequent Public Session users.
    // OK to allow as long as it doesn't affect PS idle time detection.
    // "power",

    // Printing initiated by user anyways, which provides consent gesture.
    "printerProvider",

    // The settings exposed via the API are under admin policy control anyways.
    "privacy",

    // Admin controls network anyways.
    "proxy",

    "runtime",

    // Looking at the code, this feature is declared but used nowhere.
    // "screensaver",

    // Access serial port.  It's hard to conceive a case in which private data
    // is stored on a serial device and being read without the user's consent.
    // Minor risk of intercepting input events from serial input devices - given
    // that serial input devices are exceedingly rare, OK to allow.
    "serial",

    // Privacy sensitive URL access.
    // "sessions",

    "socket",

    // Per-app sandbox.  User cannot log into Public Session, thus storage
    // cannot be sync'ed to the cloud.
    "storage",

    // Not very useful since no signed-in user.
    "syncFileSystem",

    // Returns CPU parameters.
    "system.cpu",

    // Display parameters query/manipulation.
    "system.display",

    // Memory parameters access.
    "system.memory",

    // Enumerates network interfaces.
    "system.network",

    // Enumerates removable storage.
    "system.storage",

    // User is prompted (allow/deny) when an extension requests tabCapture. The
    // request is made via chrome.tabCapture.capture call.
    "tabCapture",

    // The URL returned by chrome.tabs API is scrubbed down to the origin.
    "tabs",

    // Privacy sensitive URL access.
    // "topSites",

    // Allows to generate TTS, but no content access. Just UX.
    "tts",

    // Might need this, but has content access. Manual whitelisting?
    // "ttsEngine",

    // Excessive resource usage is not a risk.
    "unlimitedStorage",
    "unlimited_storage",

    // Plugging the USB device is sufficient as consent gesture.
    "usb",

    // Belongs to the USB API.
    "usbDevices",

    // User is prompted (allow/deny) when an extension requests videoCapture.
    // The request is done via the getUserMedia API.
    "videoCapture",

    // Admin controls network config anyways.
    "vpnProvider",

    // Just UX.
    "wallpaper",

    // Privacy sensitive URL access.
    // "webNavigation",

    // Sensitive content is stripped away.
    "webRequest",
    "webRequestBlocking",

    // This allows content scripts and capturing. However, the webview runs
    // within a separate storage partition, i.e. doesn't share cookies and other
    // storage with the browsing session. Furthermore, the embedding app could
    // just as well proxy 3rd-party origin content through its own web origin
    // server-side or via chrome.socket. Finally, web security doesn't make a
    // lot of sense when there's no URL bar or HTTPS padlock providing trusted
    // UI. Bottom line: Risks are mitigated, further restrictions don't make
    // sense, so OK to allow.
    "webview",
};

// Some permissions take the form of a dictionary.  See |kSafePermissionStrings|
// for permission strings (and for more documentation).
const char* const kSafePermissionDicts[] = {
    // Dictionary forms of the above permission strings.
    "fileSystem",
    "mediaGalleries",
    "socket",
    "usbDevices",
};

// List of safe entries for the "app" dict in manifest.
const char* const kSafeAppStrings[] = {
    "background",
    "content_security_policy",
    "icon_color",
    "isolation",
    "launch",
    "linked_icons",
};

// Return true iff |entry| is contained in |char_array|.
bool ArrayContainsImpl(const char* const char_array[],
                       size_t entry_count,
                       const std::string& entry) {
  for (size_t i = 0; i < entry_count; ++i) {
    if (entry == char_array[i]) {
      return true;
    }
  }
  return false;
}

// See http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx for an
// explanation of array size determination.
template <size_t N>
bool ArrayContains(const char* const (&char_array)[N],
                   const std::string& entry) {
  return ArrayContainsImpl(char_array, N, entry);
}

// Helper method used to log extension permissions UMA stats.
void LogPermissionUmaStats(const std::string& permission_string) {
  const auto* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByName(permission_string);
  // Not a permission.
  if (!permission_info) return;

  base::UmaHistogramSparse("Enterprise.PublicSession.ExtensionPermissions",
                           permission_info->id());
}

// Returns true for extensions that are considered safe for Public Sessions,
// which among other things requires the manifest top-level entries to be
// contained in the |kSafeManifestEntries| whitelist and all permissions to be
// contained in |kSafePermissionStrings| or |kSafePermissionDicts|.  Otherwise
// returns false and logs all reasons for failure.
bool IsSafeForPublicSession(const extensions::Extension* extension) {
  // If Public Session restrictions are not enabled, just return true.
  if (!profiles::ArePublicSessionRestrictionsEnabled())
    return true;

  bool safe = true;
  if (!extension->is_extension() &&
      !extension->is_hosted_app() &&
      !extension->is_platform_app() &&
      !extension->is_shared_module() &&
      !extension->is_theme()) {
    LOG(ERROR) << extension->id()
               << " is not of a supported type. Extension type: "
               << extension->GetType();
    safe = false;
  }

  for (base::DictionaryValue::Iterator it(*extension->manifest()->value());
       !it.IsAtEnd(); it.Advance()) {
    if (ArrayContains(kSafeManifestEntries, it.key())) {
      continue;
    }

    // Permissions must be whitelisted in |kSafePermissionStrings| or
    // |kSafePermissionDicts|.
    if (it.key() == emk::kPermissions ||
        it.key() == emk::kOptionalPermissions) {
      const base::ListValue* list_value;
      if (!it.value().GetAsList(&list_value)) {
        LOG(ERROR) << extension->id() << ": " << it.key() << " is not a list.";
        safe = false;
        continue;
      }
      for (auto it2 = list_value->begin(); it2 != list_value->end(); ++it2) {
        // Try to read as dictionary.
        const base::DictionaryValue *dict_value;
        if (it2->GetAsDictionary(&dict_value)) {
          if (dict_value->size() != 1) {
            LOG(ERROR) << extension->id()
                       << " has dict in permission list with size "
                       << dict_value->size() << ".";
            safe = false;
            continue;
          }
          for (base::DictionaryValue::Iterator it3(*dict_value);
               !it3.IsAtEnd(); it3.Advance()) {
            // Log permission (dictionary form).
            LogPermissionUmaStats(it3.key());
            if (!ArrayContains(kSafePermissionDicts, it3.key())) {
              LOG(ERROR) << extension->id()
                         << " has non-whitelisted dict in permission list: "
                         << it3.key();
              safe = false;
              continue;
            }
          }
          continue;
        }
        // Try to read as string.
        std::string permission_string;
        if (!it2->GetAsString(&permission_string)) {
          LOG(ERROR) << extension->id() << ": " << it.key()
                     << " contains a token that's neither a string nor a dict.";
          safe = false;
          continue;
        }
        // Log permission (usual, string form).
        LogPermissionUmaStats(permission_string);
        // Accept whitelisted permissions.
        if (ArrayContains(kSafePermissionStrings, permission_string)) {
          continue;
        }
        // Web requests (origin permissions).  Don't include <all_urls> because
        // that also matches file:// schemes.
        if (base::StartsWith(permission_string, "https://",
                             base::CompareCase::SENSITIVE) ||
            base::StartsWith(permission_string, "http://",
                             base::CompareCase::SENSITIVE) ||
            base::StartsWith(permission_string, "ftp://",
                             base::CompareCase::SENSITIVE)) {
          // Allow origin permissions if the extension is isolated from the main
          // browser session (so it can't access user cookies, etc.).
          if (!extensions::AppIsolationInfo::HasIsolatedStorage(extension)) {
            LOG(ERROR) << extension->id() << " does not have isolated storage "
                       "and it requested origin permission: "
                       << permission_string;
            safe = false;
          }
          continue;
        }
        LOG(ERROR) << extension->id()
                   << " requested non-whitelisted permission: "
                   << permission_string;
        safe = false;
      }
    } else if (it.key() == emk::kApp) {
      if (!extension->is_hosted_app() &&
          !extension->is_platform_app()) {
        LOG(ERROR) << extension->id()
                   << ": app manifest entry is allowed only for hosted_app or "
                       "platform_app extension type. Current extension type: "
                   << extension->GetType();
        safe = false;
      }
      const base::DictionaryValue *dict_value;
      if (!it.value().GetAsDictionary(&dict_value)) {
        LOG(ERROR) << extension->id() << ": app is not a dictionary.";
        safe = false;
        continue;
      }
      for (base::DictionaryValue::Iterator it2(*dict_value);
           !it2.IsAtEnd(); it2.Advance()) {
        if (!ArrayContains(kSafeAppStrings, it2.key())) {
          LOG(ERROR) << extension->id()
                     << " has non-whitelisted manifest entry: "
                     << it.key() << "." << it2.key();
          safe = false;
          continue;
        }
      }
    // Require v2 because that's the only version we understand.
    } else if (it.key() == emk::kManifestVersion) {
      int version;
      if (!it.value().GetAsInteger(&version)) {
        LOG(ERROR) << extension->id() << ": " << emk::kManifestVersion
                   << " is not an integer.";
        safe = false;
        continue;
      }
      if (version != 2) {
        LOG(ERROR) << extension->id()
                   << " has non-whitelisted manifest version.";
        safe = false;
        continue;
      }
    // URL handlers depend on the web store to confirm ownership of the domain.
    } else if (it.key() == emk::kUrlHandlers) {
      if (!extension->from_webstore()) {
        LOG(ERROR) << extension->id() << " uses emk::kUrlHandlers but was not "
            "installed through the web store.";
        safe = false;
        continue;
      }
    // Everything else is an error.
    } else {
      LOG(ERROR) << extension->id()
                 << " has non-whitelisted manifest entry: " << it.key();
      safe = false;
    }
  }

  return safe;
}

}  // namespace

DeviceLocalAccountManagementPolicyProvider::
    DeviceLocalAccountManagementPolicyProvider(
        policy::DeviceLocalAccount::Type account_type)
    : account_type_(account_type) {
}

DeviceLocalAccountManagementPolicyProvider::
    ~DeviceLocalAccountManagementPolicyProvider() {
}

// static
bool DeviceLocalAccountManagementPolicyProvider::IsWhitelisted(
    const std::string& extension_id) {
  return extensions::IsWhitelistedForPublicSession(extension_id);
}

std::string DeviceLocalAccountManagementPolicyProvider::
    GetDebugPolicyProviderName() const {
#if DCHECK_IS_ON()
  return "whitelist for device-local accounts";
#else
  IMMEDIATE_CRASH();
#endif
}

bool DeviceLocalAccountManagementPolicyProvider::UserMayLoad(
    const extensions::Extension* extension,
    base::string16* error) const {
  if (account_type_ == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION ||
      account_type_ == policy::DeviceLocalAccount::TYPE_SAML_PUBLIC_SESSION) {
    // Allow extension if it is a component of Chrome.
    if (extension->location() == extensions::Manifest::EXTERNAL_COMPONENT ||
        extension->location() == extensions::Manifest::COMPONENT) {
      return true;
    }

    // TODO(isandrk): Remove when whitelisting work is done (crbug/651027).
    // Allow extension if its type is whitelisted for use in public sessions.
    if (extension->GetType() == extensions::Manifest::TYPE_HOSTED_APP) {
      return true;
    }

    // Allow extension if its specific ID is whitelisted for use in public
    // sessions.
    if (IsWhitelisted(extension->id())) {
      return true;
    }

    // Allow force-installed extension if all manifest contents are whitelisted.
    if ((extension->location() == extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD
         || extension->location() == extensions::Manifest::EXTERNAL_POLICY)
        && IsSafeForPublicSession(extension)) {
      return true;
    }
  } else if (account_type_ == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    // For single-app kiosk sessions, allow platform apps, extesions and shared
    // modules.
    if (extension->GetType() == extensions::Manifest::TYPE_PLATFORM_APP ||
        extension->GetType() == extensions::Manifest::TYPE_SHARED_MODULE ||
        extension->GetType() == extensions::Manifest::TYPE_EXTENSION) {
      return true;
    }
  } else if (account_type_ == policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP) {
    if (extension->GetType() == extensions::Manifest::TYPE_EXTENSION) {
      return true;
    }
  }

  // Disallow all other extensions.
  if (error) {
    *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_IN_DEVICE_LOCAL_ACCOUNT,
          base::UTF8ToUTF16(extension->name()),
          base::UTF8ToUTF16(extension->id()));
  }
  return false;
}

}  // namespace chromeos
