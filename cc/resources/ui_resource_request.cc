// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_request.h"

#include "base/memory/ptr_util.h"

namespace cc {

UIResourceRequest::UIResourceRequest(Type type, UIResourceId id)
    : type_(type), id_(id) {
  DCHECK_EQ(type, Type::kDelete);
}

UIResourceRequest::UIResourceRequest(Type type,
                                     UIResourceId id,
                                     const UIResourceBitmap& bitmap)
    : type_(type), id_(id), bitmap_(new UIResourceBitmap(bitmap)) {}

UIResourceRequest::UIResourceRequest(const UIResourceRequest& request) {
  (*this) = request;
}

UIResourceRequest& UIResourceRequest::operator=(
    const UIResourceRequest& request) {
  type_ = request.type_;
  id_ = request.id_;
  if (request.bitmap_) {
    bitmap_ = base::WrapUnique(new UIResourceBitmap(*request.bitmap_.get()));
  } else {
    bitmap_ = nullptr;
  }

  return *this;
}

UIResourceRequest::~UIResourceRequest() = default;

}  // namespace cc
