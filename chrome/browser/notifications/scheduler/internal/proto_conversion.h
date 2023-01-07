// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_PROTO_CONVERSION_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_PROTO_CONVERSION_H_

#include "chrome/browser/notifications/proto/client_state.pb.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/proto/notification_entry.pb.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"

namespace notifications {

// Converts an icon entry to icon proto.
void IconEntryToProto(IconEntry* entry, notifications::proto::Icon* proto);

// Converts an icon proto to icon entry.
void IconEntryFromProto(proto::Icon* proto, notifications::IconEntry* entry);

// Converts client state to proto.
void ClientStateToProto(ClientState* client_state,
                        notifications::proto::ClientState* proto);

// Converts proto to client state.
void ClientStateFromProto(proto::ClientState* proto,
                          notifications::ClientState* client_state);

// Converts notification entry to proto.
void NotificationEntryToProto(NotificationEntry* entry,
                              proto::NotificationEntry* proto);

// Converts proto to notification entry.
void NotificationEntryFromProto(proto::NotificationEntry* proto,
                                NotificationEntry* entry);

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_PROTO_CONVERSION_H_
