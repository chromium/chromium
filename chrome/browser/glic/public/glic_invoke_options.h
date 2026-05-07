// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INVOKE_OPTIONS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INVOKE_OPTIONS_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace glic {
class GlicInstance;

// Use ongoing conversation for the tab if it exists. Otherwise, fall back
// to the default behavior for opening the UI (typically a new conversation).
struct DefaultConversation {};

// Always invoke into a new conversation.
struct NewConversation {};

// Use the conversation with the given ID and optionally a specific turn ID.
struct ConversationId {
  explicit ConversationId(std::string conversation_id);
  ConversationId(std::string conversation_id,
                 std::optional<std::string> turn_id);
  ~ConversationId();
  ConversationId(const ConversationId&);
  ConversationId& operator=(const ConversationId&);

  std::string conversation_id;
  std::optional<std::string> turn_id;
};

// Use the default surface (active tab of specified window or a new window).
struct DefaultSurface {
  raw_ptr<BrowserWindowInterface> browser = nullptr;
};

// Create a new tab in the specified window, or a new window if null.
struct NewTab {
  raw_ptr<BrowserWindowInterface> window = nullptr;
  bool open_in_foreground = true;
};
// The target for the invocation.
struct Target {
  Target();
  explicit Target(tabs::TabInterface* tab);
  explicit Target(BrowserWindowInterface* window);
  explicit Target(NewTab new_tab);
  Target(tabs::TabInterface* tab,
         std::variant<DefaultConversation, NewConversation, ConversationId>
             conversation);
  explicit Target(
      std::variant<DefaultConversation, NewConversation, ConversationId>
          conversation);
  Target(Target&&);
  Target& operator=(Target&&);
  ~Target();

  // Specifies the surface where Glic should be invoked.
  // - DefaultSurface: Resolves to the active tab of the specified browser
  //   window, or creates a new window if no browser is specified.
  // - NewTab: Creates a new tab in the specified window, or a new window if
  // null.
  // - TabInterface*: Targets a specific tab. Must not be null.
  std::variant<DefaultSurface, NewTab, raw_ptr<tabs::TabInterface>> surface =
      DefaultSurface();

  // Specifies which conversation to use or create.
  // - DefaultConversation: Uses the conversation already bound to the target
  //   surface if available, otherwise creates a new one.
  // - NewConversation: Forces the creation of a new conversation.
  // - ConversationId: Reconnects to a specific existing conversation.
  std::variant<DefaultConversation, NewConversation, ConversationId>
      conversation = DefaultConversation();

  // Specifies the target for actuation.
  mojom::ActuationTarget actuation_target =
      mojom::ActuationTarget::kAgentDecides;
};

// Configuration to override the default ZSS behavior for the invocation,
// only having an impact if ZSS would be shown for the invocation.
struct ZssConfig {
  ZssConfig();
  explicit ZssConfig(std::optional<std::string> additional_content);
  ~ZssConfig();
  ZssConfig(const ZssConfig&);
  ZssConfig& operator=(const ZssConfig&);

  // Additional content to inject into the body of the ZSS message.
  std::optional<std::string> additional_content;
};

// The level of in-flight navigation events allowed without canceling the
// invocation.
enum class AllowedInflightNavigation {
  kNone,
  kSameDomain,
  kAll,
};

// Possible errors that can occur during a Glic invocation.
enum class GlicInvokeError {
  kUnknown,
  // The invocation timed out before completion.
  kTimeout,
  // The provided conversation ID was invalid (e.g. empty).
  kInvalidConversationId,
  // The provided tab was invalid (e.g. null).
  kInvalidTab,
  // The tab was closed before the invocation could complete.
  kTabClosed,
  // The instance was destroyed before the invocation could complete.
  kInstanceDestroyed,
  // The instance is already handling an invocation.
  kInvokeInProgress,
  // The provided invocation configuration is invalid.
  kInvalidConfiguration,
};

// Details for invoking Glic with tabs shared. See
// GlicSharingManager::PinTabs().
struct TabSharingOptions {
  TabSharingOptions();
  TabSharingOptions(std::vector<tabs::TabHandle> tabs_to_pin,
                    GlicPinTrigger pin_trigger);
  TabSharingOptions(TabSharingOptions&&);
  TabSharingOptions& operator=(TabSharingOptions&&);
  ~TabSharingOptions();

  // Tabs to pin.
  std::vector<tabs::TabHandle> tabs_to_pin;

  // Reason for pinning tabs, required to be set to something besides kUnknown
  // if `tabs_to_pin` isn't empty.
  GlicPinTrigger pin_trigger;
};

// Configuration options for invoking Glic.
struct GlicInvokeOptions {
  explicit GlicInvokeOptions(glic::mojom::InvocationSource invocation_source);
  GlicInvokeOptions(Target target,
                    glic::mojom::InvocationSource invocation_source);
  GlicInvokeOptions(GlicInvokeOptions&&);
  GlicInvokeOptions& operator=(GlicInvokeOptions&&);
  ~GlicInvokeOptions();

  // A unique identifier for the invocation source. Primarily used for
  // logging, metrics collection, and special-case client routing.
  glic::mojom::InvocationSource invocation_source;

  // One or more pre-determined prompts to offer or submit. Providing multiple
  // prompts can facilitate a chip-style UI on the client.
  std::vector<std::string> prompts;

  // Additional context (e.g., image data, Annotated Page Content) to be
  // included with the invocation.
  // Warning: not fully implemented.
  // TODO(b/504627812): finish implementing.
  glic::mojom::AdditionalContextPtr additional_context;

  // Tabs to pin as part of invocation.
  TabSharingOptions tab_sharing;

  // Defines the target for the invocation (surface and conversation).
  Target target;

  // The feature mode to use for the invocation, triggering specific client
  // behaviours like actuation or image generation.
  std::optional<glic::mojom::FeatureMode> feature_mode;

  // Whether to suppress the Zero State Suggestions (ZSS) feature for security,
  // privacy, or UX reasons.
  bool disable_zss = false;

  // Configuration to override the default ZSS behavior for the invocation,
  // only having an impact if ZSS would be shown for the invocation.
  std::optional<ZssConfig> zss_config;

  // If this invocation is used by the skill feature, this specifies its ID.
  std::optional<std::string> skill_id;

  // The FRE override, if any.
  glic::mojom::FreOverride fre_override =
      glic::mojom::FreOverride::kUnspecified;

  // A custom string message to show the user if something goes wrong.
  std::optional<std::string> error_message;

  // The amount of time to wait before canceling the invocation.
  std::optional<base::TimeDelta> timeout;

  // The level of navigation events allowed without canceling the invocation.
  AllowedInflightNavigation allowed_inflight_navigation =
      AllowedInflightNavigation::kAll;

  // Whether to wait until the side panel has fully opened and the web
  // contents have stabilized before sending the invoke payload to the client.
  // Defaults to false. If the panel was already open when the invoke was
  // triggered, this flag is ignored.
  bool wait_for_panel_open = false;

  // Browser-specific callback for when the invocation successfully completes.
  // This is called asynchronously.
  base::OnceClosure on_success;

  // Browser-specific callback for when the web client connects (i.e., the
  // initialization handshake with the web client is complete).
  base::OnceCallback<void(base::WeakPtr<GlicInstance>)> on_client_connected;

  // Browser-specific callback for when the invocation fails.
  // This is called asynchronously.
  base::OnceCallback<void(GlicInvokeError)> on_error;
};

// Configuration options for invoking Glic with auto-submit.
struct GlicInvokeWithAutoSubmitOptions {
  GlicInvokeWithAutoSubmitOptions();
  ~GlicInvokeWithAutoSubmitOptions();
  GlicInvokeWithAutoSubmitOptions(GlicInvokeWithAutoSubmitOptions&&);
  GlicInvokeWithAutoSubmitOptions& operator=(GlicInvokeWithAutoSubmitOptions&&);

  // Callback for when the conversation ID is known.
  base::OnceCallback<void(std::string)> on_conversation_id_ready;

  // Whether or not to show the panel on invocation. Doesn't prevent the panel
  // from being shown later and has no impact if the panel is already showing.
  bool show_panel = true;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INVOKE_OPTIONS_H_
