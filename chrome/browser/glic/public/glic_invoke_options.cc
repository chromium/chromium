// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

ConversationId::ConversationId(std::string conversation_id)
    : conversation_id(std::move(conversation_id)) {}
ConversationId::ConversationId(std::string conversation_id,
                               std::optional<std::string> turn_id)
    : conversation_id(std::move(conversation_id)),
      turn_id(std::move(turn_id)) {}
ConversationId::~ConversationId() = default;
ConversationId::ConversationId(const ConversationId&) = default;
ConversationId& ConversationId::operator=(const ConversationId&) = default;

ZssConfig::ZssConfig() = default;
ZssConfig::ZssConfig(std::optional<std::string> additional_content)
    : additional_content(std::move(additional_content)) {}
ZssConfig::~ZssConfig() = default;
ZssConfig::ZssConfig(const ZssConfig&) = default;
ZssConfig& ZssConfig::operator=(const ZssConfig&) = default;

Target::Target() = default;
Target::Target(Target&&) = default;
Target& Target::operator=(Target&&) = default;
Target::~Target() = default;

Target::Target(tabs::TabInterface* tab) : surface(tab) {}

Target::Target(BrowserWindowInterface* window) : surface(NewTab{window}) {}

Target::Target(NewTab new_tab) : surface(std::move(new_tab)) {}

Target::Target(
    tabs::TabInterface* tab,
    std::variant<DefaultConversation, NewConversation, ConversationId>
        conversation)
    : surface(tab), conversation(std::move(conversation)) {}

Target::Target(
    std::variant<DefaultConversation, NewConversation, ConversationId>
        conversation)
    : conversation(std::move(conversation)) {}

TabSharingOptions::TabSharingOptions()
    : pin_trigger(GlicPinTrigger::kUnknown) {}
TabSharingOptions::TabSharingOptions(std::vector<tabs::TabHandle> tabs_to_pin,
                                     GlicPinTrigger pin_trigger)
    : tabs_to_pin(std::move(tabs_to_pin)), pin_trigger(pin_trigger) {}
TabSharingOptions::TabSharingOptions(TabSharingOptions&&) = default;
TabSharingOptions& TabSharingOptions::operator=(TabSharingOptions&&) = default;
TabSharingOptions::~TabSharingOptions() = default;

GlicInvokeOptions::GlicInvokeOptions(
    glic::mojom::InvocationSource invocation_source)
    : invocation_source(invocation_source) {}

GlicInvokeOptions::GlicInvokeOptions(
    Target target,
    glic::mojom::InvocationSource invocation_source)
    : invocation_source(invocation_source), target(std::move(target)) {}

GlicInvokeOptions::~GlicInvokeOptions() = default;

GlicInvokeOptions::GlicInvokeOptions(GlicInvokeOptions&&) = default;

GlicInvokeOptions& GlicInvokeOptions::operator=(GlicInvokeOptions&&) = default;

GlicInvokeWithAutoSubmitOptions::GlicInvokeWithAutoSubmitOptions() = default;
GlicInvokeWithAutoSubmitOptions::~GlicInvokeWithAutoSubmitOptions() = default;
GlicInvokeWithAutoSubmitOptions::GlicInvokeWithAutoSubmitOptions(
    GlicInvokeWithAutoSubmitOptions&&) = default;
GlicInvokeWithAutoSubmitOptions& GlicInvokeWithAutoSubmitOptions::operator=(
    GlicInvokeWithAutoSubmitOptions&&) = default;

}  // namespace glic
