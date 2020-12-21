// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_UTIL_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/token.h"
#include "chrome/browser/chromeos/crosapi/environment_provider.h"
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

// These methods are used by ash-chrome.
namespace crosapi {
namespace browser_util {

extern const base::Feature kLacrosAllowOnStableChannel;

// A command-line switch that can also be set from chrome://flags that affects
// the frequency of Lacros updates.
extern const char kLacrosStabilitySwitch[];
extern const char kLacrosStabilityLessStable[];
extern const char kLacrosStabilityMoreStable[];

// Boolean preference. Whether to launch lacros-chrome on login.
extern const char kLaunchOnLoginPref[];

// Registers user profile preferences related to the lacros-chrome binary.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the user directory for lacros-chrome.
base::FilePath GetUserDataDir();

// Returns true if the Lacros feature is enabled and Lacros is allowed for the
// current user type, chrome channel, and enterprise policy.
bool IsLacrosEnabled();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosEnabled(version_info::Channel channel);

// Forces IsLacrosEnabled() to return true for testing.
void SetLacrosEnabledForTest(bool force_enabled);

// Returns true if |window| is an exo ShellSurface window representing a Lacros
// browser.
bool IsLacrosWindow(const aura::Window* window);

// Returns the UUID and version for all tracked interfaces. Exposed for testing.
base::flat_map<base::Token, uint32_t> GetInterfaceVersions();

// Invite the lacros-chrome to the mojo universe.
// Queue messages to establish the mojo connection, so that the passed IPC is
// available already when lacros-chrome accepts the invitation.
mojo::Remote<crosapi::mojom::LacrosChromeService>
SendMojoInvitationToLacrosChrome(
    ::crosapi::EnvironmentProvider* environment_provider,
    mojo::PlatformChannelEndpoint local_endpoint,
    base::OnceClosure mojo_disconnected_callback,
    base::OnceCallback<
        void(mojo::PendingReceiver<crosapi::mojom::AshChromeService>)>
        ash_chrome_service_callback);

// Creates a memory backed file containing the serialized |params|,
// and returns its FD.
base::ScopedFD CreateStartupData(
    ::crosapi::EnvironmentProvider* environment_provider);

}  // namespace browser_util
}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_UTIL_H_
