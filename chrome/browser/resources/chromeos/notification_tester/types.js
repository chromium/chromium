// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Will eventually match Notification from
 * ui/message_center/public/cpp/notification.h.
 * Items prefixed with "richData" are from RichNotificationData.
 * @typedef {{
 *   id: string,
 *   title: string,
 *   message: string,
 *   icon: string,
 *   displaySource: string,
 *   originURL: string,
 *   notificationType: number,
 *   notifierType: number,
 *   warningLevel: number,
 *   richDataImage: string,
 *   richDataSmallImage: string,
 *   richDataNeverTimeout: boolean,
 *   richDataPriority: number,
 *   richDataPinned: boolean,
 *   richDataShowSnooze: boolean,
 *   richDataShowSettings: boolean,
 *   richDataProgress: number,
 *   richDataProgressStatus: string,
 *   richDataNumButtons: number,
 *   richDataNumNotifItems: number,
 * }}
 */
export let Notification;