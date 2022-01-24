// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_app_client.h"

#include "base/check_op.h"
#include "base/values.h"

namespace ash {

namespace {

constexpr char kPendingScreencastName[] = "name";
constexpr char kPendingScreencastUploadProgress[] = "uploadProgress";

ProjectorAppClient* g_instance = nullptr;
}  // namespace

base::Value PendingScreencast::ToValue() const {
  base::Value val(base::Value::Type::DICTIONARY);
  val.SetKey(kPendingScreencastName, base::Value(name));

  // TODO(b/199421317): Show uploading progress of pending screencasts in
  // gallery. Calculate and set the correct value here.
  val.SetKey(kPendingScreencastUploadProgress, base::Value(0));
  return val;
}

// TODO(b/199421317): Add transferred bytes check and show uploading progress of
// pending screencasts in gallery.
bool PendingScreencast::operator==(const PendingScreencast& rhs) const {
  return rhs.container_dir == container_dir;
}

bool PendingScreencast::operator<(const PendingScreencast& rhs) const {
  return rhs.container_dir < container_dir;
}

// static
ProjectorAppClient* ProjectorAppClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

ProjectorAppClient::ProjectorAppClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

ProjectorAppClient::~ProjectorAppClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
