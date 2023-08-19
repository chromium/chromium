// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer_iterator.h"

namespace cc {

namespace {

// When |op| is a nested PaintOpBuffer, this returns the PaintOp inside
// that buffer if the buffer contains a single drawing op, otherwise it
// returns null. This searches recursively if the PaintOpBuffer contains only
// another PaintOpBuffer.
static const PaintOp* GetNestedSingleDrawingOp(const PaintOp* op) {
  if (!op->IsDrawOp())
    return nullptr;
  while (op->GetType() == PaintOpType::DrawRecord) {
    auto* draw_record_op = static_cast<const DrawRecordOp*>(op);
    if (draw_record_op->record.size() > 1) {
      // If there's more than one op, then we need to keep the
      // SaveLayer.
      return nullptr;
    }

    // Recurse into the single-op DrawRecordOp and make sure it's a
    // drawing op.
    op = &draw_record_op->record.GetFirstOp();
    if (!op->IsDrawOp())
      return nullptr;
  }

  return op;
}

}  // anonymous namespace

PaintOpBuffer::CompositeIterator::CompositeIterator(
    const PaintOpBuffer& buffer,
    const std::vector<size_t>* offsets)
    : iter_(offsets == nullptr ? absl::variant<Iterator, OffsetIterator>(
                                     absl::in_place_type<Iterator>,
                                     buffer)
                               : absl::variant<Iterator, OffsetIterator>(
                                     absl::in_place_type<OffsetIterator>,
                                     buffer,
                                     *offsets)) {}

PaintOpBuffer::CompositeIterator::CompositeIterator(
    const CompositeIterator& other) = default;
PaintOpBuffer::CompositeIterator::CompositeIterator(CompositeIterator&& other) =
    default;

PaintOpBuffer::PlaybackFoldingIterator::PlaybackFoldingIterator(
    const PaintOpBuffer& buffer,
    const std::vector<size_t>* offsets)
    : iter_(buffer, offsets),
      folded_draw_color_(SkColors::kTransparent, SkBlendMode::kSrcOver) {
  FindNextOp();
}

PaintOpBuffer::PlaybackFoldingIterator::~PlaybackFoldingIterator() = default;

void PaintOpBuffer::PlaybackFoldingIterator::FindNextOp() {
  current_alpha_ = 1.0f;
  for (current_op_ = NextUnfoldedOp(); current_op_;
       current_op_ = NextUnfoldedOp()) {
    if (current_op_->GetType() != PaintOpType::SaveLayerAlpha)
      break;
    const PaintOp* second = NextUnfoldedOp();
    if (!second)
      break;

    if (second->GetType() == PaintOpType::Restore) {
      // Drop a SaveLayerAlpha/Restore combo.
      continue;
    }

    // Find a nested drawing PaintOp to replace |second| if possible, while
    // holding onto the pointer to |second| in case we can't find a nested
    // drawing op to replace it with.
    const PaintOp* draw_op = GetNestedSingleDrawingOp(second);

    const PaintOp* third = nullptr;
    if (draw_op) {
      third = NextUnfoldedOp();
      if (third && third->GetType() == PaintOpType::Restore) {
        auto* save_op = static_cast<const SaveLayerAlphaOp*>(current_op_);
        if (draw_op->IsPaintOpWithFlags() &&
            // SkPaint::drawTextBlob() applies alpha on each glyph so we don't
            // fold SaveLayerAlpha into DrwaTextBlob to ensure correct alpha
            // even if some glyphs overlap.
            draw_op->GetType() != PaintOpType::DrawTextBlob) {
          auto* flags_op = static_cast<const PaintOpWithFlags*>(draw_op);
          if (flags_op->flags.SupportsFoldingAlpha()) {
            current_alpha_ = save_op->alpha;
            current_op_ = draw_op;
            break;
          }
        } else if (draw_op->GetType() == PaintOpType::DrawColor &&
                   static_cast<const DrawColorOp*>(draw_op)->mode ==
                       SkBlendMode::kSrcOver) {
          auto* draw_color_op = static_cast<const DrawColorOp*>(draw_op);
          SkColor4f color = draw_color_op->color;
          folded_draw_color_.color = {color.fR, color.fG, color.fB,
                                      save_op->alpha * color.fA};
          current_op_ = &folded_draw_color_;
          break;
        }
      }
    }

    // If we get here, then we could not find a foldable sequence after
    // this SaveLayerAlpha, so store any peeked at ops.
    stack_.push_back(second);
    if (third)
      stack_.push_back(third);
    break;
  }
}

const PaintOp* PaintOpBuffer::PlaybackFoldingIterator::NextUnfoldedOp() {
  if (stack_.size()) {
    const PaintOp* op = stack_.front();
    // Shift paintops forward.
    stack_.erase(stack_.begin());
    return op;
  }
  if (!iter_)
    return nullptr;
  const PaintOp& op = *iter_;
  ++iter_;
  return &op;
}

}  // namespace cc
