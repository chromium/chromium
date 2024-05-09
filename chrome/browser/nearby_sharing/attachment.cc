// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/attachment.h"
#include "crypto/random.h"

namespace {

int64_t CreateRandomId() {
  int64_t id;
  crypto::RandBytes(base::byte_span_from_ref(id));
  return id;
}

}  // namespace

Attachment::Attachment(Family family, int64_t size)
    : id_(CreateRandomId()), family_(family), size_(size) {}

Attachment::Attachment(int64_t id, Family family, int64_t size)
    : id_(id), family_(family), size_(size) {}

Attachment::Attachment(const Attachment&) = default;

Attachment::Attachment(Attachment&&) = default;

Attachment& Attachment::operator=(const Attachment&) = default;

Attachment& Attachment::operator=(Attachment&&) = default;

Attachment::~Attachment() = default;
