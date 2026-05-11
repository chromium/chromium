// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/at_memory_accessory_controller_impl.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "content/public/browser/web_contents.h"

using ::autofill::AccessoryAction;
using ::autofill::AccessorySheetData;
using ::autofill::AccessorySheetField;
using ::autofill::BrowserAutofillManager;
using ::autofill::ContentAutofillDriver;
using ::autofill::FieldGlobalId;
using ::autofill::FindRenderFrameHostByToken;

// static
AtMemoryAccessoryController* AtMemoryAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  return AtMemoryAccessoryControllerImpl::GetOrCreateForWebContents(
      web_contents);
}

AtMemoryAccessoryControllerImpl::AtMemoryAccessoryControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<AtMemoryAccessoryControllerImpl>(
          *web_contents) {}

AtMemoryAccessoryControllerImpl::~AtMemoryAccessoryControllerImpl() = default;

void AtMemoryAccessoryControllerImpl::RegisterFillingSourceObserver(
    FillingSourceObserver observer) {
  // No-op.
}

std::optional<AccessorySheetData>
AtMemoryAccessoryControllerImpl::GetSheetData() const {
  return std::nullopt;
}

void AtMemoryAccessoryControllerImpl::OnFillingTriggered(
    FieldGlobalId focused_field_id,
    const AccessorySheetField& selection) {
  NOTREACHED() << "Unhandled filling triggered";
}

void AtMemoryAccessoryControllerImpl::OnPasskeySelected(
    const std::vector<uint8_t>& passkey_id) {
  NOTREACHED() << "Unhandled passkey selected";
}

void AtMemoryAccessoryControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) {
  if (selected_action != AccessoryAction::SHOW_AT_MEMORY_BOTTOMSHEET) {
    NOTREACHED() << "Unhandled option selected: "
                 << std::to_underlying(selected_action);
  }

  base::WeakPtr<ManualFillingController> manual_filling_controller =
      ManualFillingController::Get(&GetWebContents());
  if (!manual_filling_controller) {
    return;
  }

  FieldGlobalId field_id = manual_filling_controller->GetLastFocusedFieldId();
  content::RenderFrameHost* rfh =
      FindRenderFrameHostByToken(GetWebContents(), field_id.frame_token);
  if (!rfh) {
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!driver) {
    return;
  }

  BrowserAutofillManager* bam =
      static_cast<BrowserAutofillManager*>(&driver->GetAutofillManager());
  bam->TriggerAtMemorySuggestions(field_id);
}

void AtMemoryAccessoryControllerImpl::OnToggleChanged(
    AccessoryAction toggled_action,
    bool enabled) {
  NOTREACHED() << "Unhandled toggled action: "
               << std::to_underlying(toggled_action);
}

base::WeakPtr<AtMemoryAccessoryController>
AtMemoryAccessoryControllerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AtMemoryAccessoryControllerImpl);
