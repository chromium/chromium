// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include "base/memory/ref_counted_memory.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkStream.h"

namespace cc {

SkottieWrapper::SkottieWrapper(
    const scoped_refptr<base::RefCountedMemory>& data_stream) {
  TRACE_EVENT0("cc", "SkottieWrapper Parse");
  SkMemoryStream sk_stream(data_stream->front(), data_stream->size());
  animation_ = skottie::Animation::Make(&sk_stream);
  DCHECK(animation_);
}

SkottieWrapper::SkottieWrapper(std::unique_ptr<SkMemoryStream> stream)
    : animation_(skottie::Animation::Make(stream.get())) {
  DCHECK(animation_);
}

SkottieWrapper::~SkottieWrapper() {}

void SkottieWrapper::Draw(SkCanvas* canvas, float t, const SkRect& rect) {
  base::AutoLock lock(lock_);
  animation_->seek(t);
  animation_->render(canvas, &rect);
}

}  // namespace cc
