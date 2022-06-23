// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Will eventually match Notification from
 * ui/message_center/public/cpp/notification.h.
 * Items prefixed with "richData" are from RichNotificationData.
 * @typedef {{
 *   title: string,
 *   message: string,
 *   icon: string,
 *   richDataImage: string,
 *   richDataSmallImage: string,
 * }}
 */
export let Notification;