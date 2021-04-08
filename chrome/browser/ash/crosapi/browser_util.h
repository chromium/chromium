// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/token.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefRegistrySimple;

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace user_manager {
class User;
}  // namespace user_manager

// These methods are used by ash-chrome.
namespace crosapi {
namespace browser_util {

// Represents different options for how to launch Lacros browser. The values
// shall be consistent with the controlling policy.
enum class LacrosLaunchSwitch {
  // Indicates that the user decides whether to enable Lacros (if allowed) and
  // make it the primary browser.
  kUserChoice = 0,
  // Indicates that Lacros is not allowed to be enabled.
  kLacrosDisallowed = 1,
  // Indicates that Lacros will be enabled (if allowed). Ash browser is the
  // primary browser.
  kSideBySide = 2,
  // Similar to kSideBySide but Lacros is the primary browser.
  kLacrosPrimary = 3,
  // Indicates that Lacros (if allowed) is the only available browser. The value
  // is preserved for future use and is not supported yet.
  kLacrosOnly = 4
};

extern const base::Feature kLacrosAllowOnStableChannel;

// A command-line switch that can also be set from chrome://flags that affects
// the frequency of Lacros updates.
extern const char kLacrosStabilitySwitch[];
extern const char kLacrosStabilityLessStable[];
extern const char kLacrosStabilityMoreStable[];

// Boolean preference. Whether to launch lacros-chrome on login.
extern const char kLaunchOnLoginPref[];

// A boolean preference that records whether the user data dir has been cleared.
// We intentionally number this as we anticipate we might need to clear the user
// data dir multiple times. This preference tracks the breaking change
// introduced by account_manager in M91/M92 timeframe.
extern const char kClearUserDataDir1Pref[];

// Registers user profile preferences related to the lacros-chrome binary.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the user directory for lacros-chrome.
base::FilePath GetUserDataDir();

// Returns true if the Lacros feature is allowed to be enabled for primary user.
// This checks user type, chrome channel and enterprise policy.
bool IsLacrosAllowedToBeEnabled(version_info::Channel channel);

// Returns true if the Lacros feature is enabled for the primary user.
bool IsLacrosEnabled();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosEnabled(version_info::Channel channel);

// As above, but takes a user. It can be called before primary user is set by
// UserManager.
bool IsLacrosEnabledWithUser(const user_manager::User* user);

// Returns true if |chromeos::features::kLacrosSupport| flag is allowed.
bool IsLacrosSupportFlagAllowed(version_info::Channel channel);

// Forces IsLacrosEnabled() to return true for testing.
void SetLacrosEnabledForTest(bool force_enabled);

// Returns true if Ash browser is enabled. Returns false iff Lacros is
// enabled and is the only browser.
bool IsAshWebBrowserEnabled();

// As above, but takes a channel. Exposed for testing.
bool IsAshWebBrowserEnabled(version_info::Channel channel);

// Returns true if the lacros should be used as a primary browser.
bool IsLacrosPrimaryBrowser();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosPrimaryBrowser(version_info::Channel channel);

// Forces IsLacrosPrimaryBrowser() to return true or false for testing.
// Passing base::nullopt will reset the state.
void SetLacrosPrimaryBrowserForTest(base::Optional<bool> value);

// Returns true if the lacros can be used as a primary browser
// for the current session.
// Note that IsLacrosPrimaryBrowser may return false, even if this returns
// true, specifically, the feature is disabled by user/policy.
bool IsLacrosPrimaryBrowserAllowed(version_info::Channel channel);

// Returns true if |chromeos::features::kLacrosPrimary| flag is allowed.
bool IsLacrosPrimaryFlagAllowed(version_info::Channel channel);

// Returns true if Lacros is allowed to launch and show a window. This can
// return false if the user is using multi-signin, which is mutually exclusive
// with Lacros.
bool IsLacrosAllowedToLaunch();

// Returns true if |window| is an exo ShellSurface window representing a Lacros
// browser.
bool IsLacrosWindow(const aura::Window* window);

// Returns true if |metadata| is appropriately formatted, contains a lacros
// version, and that lacros versions supports the new backwards-incompatible
// account_manager logic.
bool DoesMetadataSupportNewAccountManager(base::Value* metadata);

// Returns the UUID and version for all tracked interfaces. Exposed for testing.
base::flat_map<base::Token, uint32_t> GetInterfaceVersions();

// Returns the initial parameter to be passed to Crosapi client,
// such as lacros-chrome.
mojom::BrowserInitParamsPtr GetBrowserInitParams(
    EnvironmentProvider* environment_provider,
    crosapi::mojom::InitialBrowserAction initial_browser_action);

// Invite the lacros-chrome to the mojo universe.
// Queue messages to establish the mojo connection, so that the passed IPC is
// available already when lacros-chrome accepts the invitation.
mojo::Remote<crosapi::mojom::BrowserService> SendMojoInvitationToLacrosChrome(
    ::crosapi::EnvironmentProvider* environment_provider,
    mojo::PlatformChannelEndpoint local_endpoint,
    base::OnceClosure mojo_disconnected_callback,
    base::OnceCallback<void(mojo::PendingReceiver<crosapi::mojom::Crosapi>)>
        crosapi_callback);

// Creates a memory backed file containing the serialized |params|,
// and returns its FD.
base::ScopedFD CreateStartupData(
    ::crosapi::EnvironmentProvider* environment_provider,
    crosapi::mojom::InitialBrowserAction initial_browser_action);

}  // namespace browser_util
}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
