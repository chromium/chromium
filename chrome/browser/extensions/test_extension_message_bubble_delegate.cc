// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_message_bubble_delegate.h"

#include "base/containers/contains.h"

namespace extensions {

TestExtensionMessageBubbleDelegate::TestExtensionMessageBubbleDelegate(
    Profile* profile)
    : ExtensionMessageBubbleController::Delegate(profile) {}
TestExtensionMessageBubbleDelegate::~TestExtensionMessageBubbleDelegate() =
    default;

bool TestExtensionMessageBubbleDelegate::ShouldIncludeExtension(
    const Extension* extension) {
  return base::Contains(extension_ids_, extension->id());
}

void TestExtensionMessageBubbleDelegate::AcknowledgeExtension(
    const ExtensionId& extension_id,
    ExtensionMessageBubbleController::BubbleAction action) {
  acknowledged_extensions_.emplace(extension_id, action);
}

void TestExtensionMessageBubbleDelegate::PerformAction(
    const ExtensionIdList& list) {}

std::u16string TestExtensionMessageBubbleDelegate::GetTitle() const {
  return u"Title";
}

std::u16string TestExtensionMessageBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  return u"Body";
}

std::u16string TestExtensionMessageBubbleDelegate::GetOverflowText(
    const std::u16string& overflow_count) const {
  return std::u16string();
}

std::u16string TestExtensionMessageBubbleDelegate::GetLearnMoreLabel() const {
  return u"Learn more";
}

GURL TestExtensionMessageBubbleDelegate::GetLearnMoreUrl() const {
  return GURL();
}

std::u16string TestExtensionMessageBubbleDelegate::GetActionButtonLabel()
    const {
  return u"OK";
}

std::u16string TestExtensionMessageBubbleDelegate::GetDismissButtonLabel()
    const {
  return u"Cancel";
}

bool TestExtensionMessageBubbleDelegate::ShouldCloseOnDeactivate() const {
  return true;
}

bool TestExtensionMessageBubbleDelegate::ShouldShow(
    const ExtensionIdList& extensions) const {
  return true;
}

void TestExtensionMessageBubbleDelegate::OnShown(
    const ExtensionIdList& extensions) {}

void TestExtensionMessageBubbleDelegate::ClearProfileSetForTesting() {}

bool TestExtensionMessageBubbleDelegate::ShouldShowExtensionList() const {
  return true;
}

bool TestExtensionMessageBubbleDelegate::ShouldLimitToEnabledExtensions()
    const {
  return true;
}

bool TestExtensionMessageBubbleDelegate::SupportsPolicyIndicator() {
  return true;
}

void TestExtensionMessageBubbleDelegate::IncludeExtensionId(
    const ExtensionId& extension_id) {
  extension_ids_.insert(extension_id);
}

bool TestExtensionMessageBubbleDelegate::WasExtensionAcknowledged(
    const ExtensionId& extension_id) const {
  return base::Contains(acknowledged_extensions_, extension_id);
}

}  // namespace extensions
