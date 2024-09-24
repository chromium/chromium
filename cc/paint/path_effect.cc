// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/path_effect.h"

#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/skia/include/core/SkPathEffect.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"

namespace cc {

namespace {

template <typename T>
bool AreEqualForTesting(const PathEffect& a, const PathEffect& b) {
  return static_cast<const T&>(a).EqualsForTesting(  // IN-TEST
      static_cast<const T&>(b));
}

class DashPathEffect final : public PathEffect {
 public:
  explicit DashPathEffect(const float intervals[], int count, float phase)
      : PathEffect(Type::kDash),
        intervals_(intervals, intervals + count),
        phase_(phase) {}

  bool EqualsForTesting(const DashPathEffect& other) const {
    return phase_ == other.phase_ && intervals_ == other.intervals_;
  }

 private:
  size_t SerializedDataSize() const override {
    return (base::CheckedNumeric<size_t>(
                PaintOpWriter::SerializedSizeOfElements(intervals_.data(),
                                                        intervals_.size())) +
            PaintOpWriter::SerializedSize(phase_))
        .ValueOrDie();
  }
  void SerializeData(PaintOpWriter& writer) const override {
    // This serialization is identical to the behavior of
    // PaintOpWriter::Write(std::vector), which lets us use
    // PaintOpReader::Read(std::vector) below.
    writer.WriteSize(intervals_.size());
    writer.WriteData(intervals_.size() * sizeof(float), intervals_.data());
    writer.Write(phase_);
  }

  sk_sp<SkPathEffect> GetSkPathEffect() const override {
    return SkDashPathEffect::Make(
        intervals_.data(), base::checked_cast<int>(intervals_.size()), phase_);
  }

  size_t dash_interval_count() const override { return intervals_.size(); }

  absl::InlinedVector<float, 2> intervals_;
  float phase_;
};

class CornerPathEffect final : public PathEffect {
 public:
  explicit CornerPathEffect(float radius)
      : PathEffect(Type::kCorner), radius_(radius) {}

  bool EqualsForTesting(const CornerPathEffect& other) const {
    return radius_ == other.radius_;
  }

 private:
  size_t SerializedDataSize() const override {
    return PaintOpWriter::SerializedSize(radius_);
  }
  void SerializeData(PaintOpWriter& writer) const override {
    writer.Write(radius_);
  }

  sk_sp<SkPathEffect> GetSkPathEffect() const override {
    return SkCornerPathEffect::Make(radius_);
  }
  float radius_;
};

}  // namespace

PathEffect::PathEffect(Type type) : type_(type) {
  DCHECK_NE(type, Type::kNull);
}

sk_sp<PathEffect> PathEffect::MakeDash(const float* intervals,
                                       int count,
                                       float phase) {
  return sk_make_sp<DashPathEffect>(intervals, count, phase);
}

sk_sp<PathEffect> PathEffect::MakeCorner(float radius) {
  return sk_make_sp<CornerPathEffect>(radius);
}

size_t PathEffect::dash_interval_count() const {
  return 0;
}

bool PathEffect::EqualsForTesting(const PathEffect& other) const {
  if (type_ != other.type_) {
    return false;
  }

  switch (type_) {
    case Type::kNull:
      return true;
    case Type::kDash:
      return AreEqualForTesting<DashPathEffect>(*this, other);
    case Type::kCorner:
      return AreEqualForTesting<CornerPathEffect>(*this, other);
  }
  NOTREACHED();
}

sk_sp<PathEffect> PathEffect::Deserialize(PaintOpReader& reader, Type type) {
  switch (type) {
    case Type::kDash: {
      std::vector<float> intervals;
      float phase;
      reader.Read(&intervals);
      reader.Read(&phase);
      return reader.valid()
                 ? MakeDash(intervals.data(),
                            base::checked_cast<int>(intervals.size()), phase)
                 : nullptr;
    }
    case Type::kCorner: {
      float radius;
      reader.Read(&radius);
      return reader.valid() ? MakeCorner(radius) : nullptr;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace cc
