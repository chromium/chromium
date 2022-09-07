// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/ime/arc_ime_bridge_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {
namespace {

mojom::SegmentStyle GetSegmentStyle(const ui::ImeTextSpan& ime_text_span) {
  if (ime_text_span.thickness == ui::ImeTextSpan::Thickness::kNone ||
      ime_text_span.underline_style == ui::ImeTextSpan::UnderlineStyle::kNone) {
    return mojom::SegmentStyle::NONE;
  }
  if (ime_text_span.thickness == ui::ImeTextSpan::Thickness::kThick) {
    return mojom::SegmentStyle::EMPHASIZED;
  }
  return mojom::SegmentStyle::DEFAULT;
}

std::vector<mojom::CompositionSegmentPtr> ConvertSegments(
    const ui::CompositionText& composition) {
  std::vector<mojom::CompositionSegmentPtr> segments;
  for (const ui::ImeTextSpan& ime_text_span : composition.ime_text_spans) {
    mojom::CompositionSegmentPtr segment = mojom::CompositionSegment::New();
    segment->start_offset = ime_text_span.start_offset;
    segment->end_offset = ime_text_span.end_offset;
    segment->style = GetSegmentStyle(ime_text_span);
    segments.push_back(std::move(segment));
  }
  return segments;
}

// Converts mojom::TEXT_INPUT_FLAG_* to ui::TextInputFlags.
int ConvertTextInputFlags(int32_t flags) {
  if (flags & mojom::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE)
    return ui::TextInputFlags::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE;
  if (flags & mojom::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS)
    return ui::TextInputFlags::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS;
  if (flags & mojom::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS)
    return ui::TextInputFlags::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS;
  return ui::TextInputFlags::TEXT_INPUT_FLAG_NONE;
}

}  // namespace

ArcImeBridgeImpl::ArcImeBridgeImpl(Delegate* delegate,
                                   ArcBridgeService* bridge_service)
    : delegate_(delegate), bridge_service_(bridge_service) {
  bridge_service_->ime()->SetHost(this);
}

ArcImeBridgeImpl::~ArcImeBridgeImpl() {
  bridge_service_->ime()->SetHost(nullptr);
}

void ArcImeBridgeImpl::SendSetCompositionText(
    const ui::CompositionText& composition) {
  auto* ime_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(), SetCompositionText);
  if (!ime_instance)
    return;

  ime_instance->SetCompositionText(base::UTF16ToUTF8(composition.text),
                                   ConvertSegments(composition),
                                   composition.selection);
}

void ArcImeBridgeImpl::SendConfirmCompositionText() {
  auto* ime_instance = ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(),
                                                   ConfirmCompositionText);
  if (!ime_instance)
    return;

  ime_instance->ConfirmCompositionText();
}

void ArcImeBridgeImpl::SendSelectionRange(const gfx::Range& selection_range) {
  auto* ime_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(), SetSelectionText);
  if (!ime_instance)
    return;

  ime_instance->SetSelectionText(selection_range);
}

void ArcImeBridgeImpl::SendInsertText(const std::u16string& text,
                                      int new_cursor_position) {
  auto* ime_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(), InsertText);
  if (!ime_instance)
    return;

  ime_instance->InsertText(base::UTF16ToUTF8(text), new_cursor_position);
}

void ArcImeBridgeImpl::SendExtendSelectionAndDelete(size_t before,
                                                    size_t after) {
  auto* ime_instance = ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(),
                                                   ExtendSelectionAndDelete);
  if (!ime_instance)
    return;

  ime_instance->ExtendSelectionAndDelete(before, after);
}

void ArcImeBridgeImpl::SendOnKeyboardAppearanceChanging(
    const gfx::Rect& new_bounds,
    bool is_available) {
  auto* ime_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->ime(), OnKeyboardAppearanceChanging);
  if (!ime_instance)
    return;

  ime_instance->OnKeyboardAppearanceChanging(new_bounds, is_available);
}

void ArcImeBridgeImpl::SendSetComposingRegion(
    const gfx::Range& composing_range) {
  auto* ime_instance =
      ARC_GET_INSTANCE_FOR_METHOD(bridge_service_->ime(), SetComposingRegion);
  if (!ime_instance)
    return;

  ime_instance->SetComposingRegion(composing_range);
}

void ArcImeBridgeImpl::OnTextInputTypeChanged(
    ui::TextInputType type,
    bool is_personalized_learning_allowed,
    int32_t flags) {
  delegate_->OnTextInputTypeChanged(type, is_personalized_learning_allowed,
                                    ConvertTextInputFlags(flags));
}

void ArcImeBridgeImpl::OnCursorRectChanged(
    const gfx::Rect& rect,
    mojom::CursorCoordinateSpace coordinate_space) {
  delegate_->OnCursorRectChanged(rect, coordinate_space);
}

void ArcImeBridgeImpl::OnCancelComposition() {
  delegate_->OnCancelComposition();
}

void ArcImeBridgeImpl::ShowVirtualKeyboardIfEnabled() {
  delegate_->ShowVirtualKeyboardIfEnabled();
}

void ArcImeBridgeImpl::OnCursorRectChangedWithSurroundingText(
    const gfx::Rect& rect,
    const gfx::Range& text_range,
    const std::string& text_in_range,
    const gfx::Range& selection_range,
    mojom::CursorCoordinateSpace coordinate_space) {
  delegate_->OnCursorRectChangedWithSurroundingText(
      rect, text_range, base::UTF8ToUTF16(text_in_range), selection_range,
      coordinate_space);
}

void ArcImeBridgeImpl::SendKeyEvent(std::unique_ptr<ui::KeyEvent> key_event,
                                    SendKeyEventCallback callback) {
  delegate_->SendKeyEvent(std::move(key_event), std::move(callback));
}

}  // namespace arc
