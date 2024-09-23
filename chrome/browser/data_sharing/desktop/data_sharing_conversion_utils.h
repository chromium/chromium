// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_CONVERSION_UTILS_H_
#define CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_CONVERSION_UTILS_H_

#include "components/data_sharing/public/protocol/group_data.mojom.h"
#include "components/data_sharing/public/protocol/group_data.pb.h"

// Utility to convert group data from mojom to protobuf.
namespace data_sharing {

data_sharing_pb::MemberRole ConvertMemberRole(
    data_sharing::mojom::MemberRole role);

data_sharing_pb::GroupMember ConvertGroupMember(
    const data_sharing::mojom::GroupMemberPtr& member);

data_sharing_pb::GroupData ConvertGroup(
    const data_sharing::mojom::GroupDataPtr& group_data);

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_CONVERSION_UTILS_H_
