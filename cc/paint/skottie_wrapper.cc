// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace {

// Directs logs from the skottie animation builder to //base/logging. Without
// this, errors/warnings from the animation builder get silently dropped.
class SkottieLogWriter : public skottie::Logger {
 public:
  void log(Level level, const char message[], const char* json) override {
    static constexpr char kSkottieLogPrefix[] = "[Skottie] \"";
    static constexpr char kSkottieLogSuffix[] = "\"";
    switch (level) {
      case Level::kWarning:
        LOG(WARNING) << kSkottieLogPrefix << message << kSkottieLogSuffix;
        break;
      case Level::kError:
        LOG(ERROR) << kSkottieLogPrefix << message << kSkottieLogSuffix;
        break;
    }
  }
};

}  // namespace

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateSerializable(
    std::vector<uint8_t> data,
    sk_sp<skresources::ResourceProvider> resource_provider) {
  base::span<const uint8_t> data_span(data);
  return base::WrapRefCounted<SkottieWrapper>(new SkottieWrapper(
      data_span, std::move(data), std::move(resource_provider)));
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateNonSerializable(
    base::span<const uint8_t> data,
    sk_sp<skresources::ResourceProvider> resource_provider) {
  return base::WrapRefCounted<SkottieWrapper>(
      new SkottieWrapper(data, /*owned_data=*/std::vector<uint8_t>(),
                         std::move(resource_provider)));
}

SkottieWrapper::SkottieWrapper(
    base::span<const uint8_t> data,
    std::vector<uint8_t> owned_data,
    sk_sp<skresources::ResourceProvider> resource_provider)
    : animation_(
          skottie::Animation::Builder()
              .setLogger(sk_make_sp<SkottieLogWriter>())
              .setResourceProvider(skresources::CachingResourceProvider::Make(
                  std::move(resource_provider)))
              .make(reinterpret_cast<const char*>(data.data()), data.size())),
      raw_data_(std::move(owned_data)),
      id_(base::FastHash(data)) {}

SkottieWrapper::~SkottieWrapper() = default;

void SkottieWrapper::Draw(SkCanvas* canvas, float t, const SkRect& rect) {
  base::AutoLock lock(lock_);
  animation_->seek(t);
  animation_->render(canvas, &rect);
}

base::span<const uint8_t> SkottieWrapper::raw_data() const {
  DCHECK(raw_data_.size());
  return base::as_bytes(base::make_span(raw_data_.data(), raw_data_.size()));
}

}  // namespace cc
