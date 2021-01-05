// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_content.pb.h"

ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
GetPersistedStateProfileProtoDBFactory() {
  static base::NoDestructor<
      ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>>
      instance;
  return instance.get();
}

template <>
ProfileProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
ProfileProtoDBFactory<
    persisted_state_db::PersistedStateContentProto>::GetInstance() {
  return GetPersistedStateProfileProtoDBFactory();
}
