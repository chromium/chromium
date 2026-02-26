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
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// Use ongoing conversation for the tab if it exists. Otherwise, fall back
// to the default behavior for opening the UI (typically a new conversation).
struct DefaultConversation {};

// Always invoke into a new conversation.
struct NewConversation {};

// Use the conversation with the given ID.
using ConversationId = std::string;

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
};

// Configuration options for invoking Glic.
struct GlicInvokeOptions {
  explicit GlicInvokeOptions(glic::mojom::InvocationSource invocation_source);
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
  glic::mojom::AdditionalContextPtr additional_context;

  // Defines the conversation this invocation targets: either a specific
  // conversation ID, or a general selection mode.
  std::variant<DefaultConversation, NewConversation, ConversationId>
      conversation = DefaultConversation();

  // Whether or not to automatically submit the conversation turn.
  // Note: This is "best effort", not all invocations (e.g., multi-prompts)
  // support automatic submission.
  bool auto_submit = false;

  // The feature mode to use for the invocation, triggering specific client
  // behaviours like actuation or image generation.
  std::optional<glic::mojom::FeatureMode> feature_mode;

  // Whether to suppress the Zero State Suggestions (ZSS) feature for security,
  // privacy, or UX reasons.
  bool disable_zss = false;

  // If this invocation is used by the skill feature, this specifies its ID.
  std::optional<std::string> skill_id;

  // A custom string message to show the user if something goes wrong.
  std::optional<std::string> error_message;

  // The amount of time to wait before canceling the invocation.
  std::optional<base::TimeDelta> timeout;

  // The level of navigation events allowed without canceling the invocation.
  AllowedInflightNavigation allowed_inflight_navigation =
      AllowedInflightNavigation::kAll;

  // Browser-specific callback for when the invocation successfully completes.
  base::OnceClosure on_success;

  // Browser-specific callback for when the invocation fails.
  base::OnceCallback<void(GlicInvokeError)> on_error;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INVOKE_OPTIONS_H_
