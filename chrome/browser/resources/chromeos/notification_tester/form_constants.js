// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * @fileoverview Stores the content of the notification tester form select
 * elements.
 * @externs
 */

/*
  Matches SystemNotificationWarningLevel in
  ui/message_center/public/cpp/notification.h
  @enum {number}
*/
export const SystemNotificationWarningLevel = {
  NORMAL: 0,
  WARNING: 1,
  CRITICAL_WARNING: 2,
};

/*
  Matches NotificationPriority in
  ui/message_center/public/cpp/notification_types.h
  @enum {number}
*/
export const NotificationPriority = {
  MIN_PRIORITY: -2,
  LOW_PRIORITY: -1,
  DEFAULT_PRIORITY: 0,
  HIGH_PRIORITY: 1,
  MAX_PRIORITY: 2,
  SYSTEM_PRIORITY: 3,
};

/*
  Matches NotificationType in
  ui/message_center/public/cpp/notification_types.h
  @enum {number}
*/
export const NotificationType = {
  NOTIFICATION_TYPE_SIMPLE: 0,
  NOTIFICATION_TYPE_BASE_FORMAT: 1,
  NOTIFICATION_TYPE_IMAGE: 2,
  NOTIFICATION_TYPE_MULTIPLE: 3,
  NOTIFICATION_TYPE_PROGRESS: 4,
};

/*
  Matches NotifierType in
  ui/message_center/public/cpp/notifier_id.h
  @enum {number}
*/
export const NotifierType = {
  WEB_PAGE: 2,
  SYSTEM_COMPONENT: 3,
};

/*
 * Stores the content of the notification tester form select
 * elements. Options correspond to the naming scheme defined in
 * ui/message_center/public/cpp/notification.h.
 * @enum {Array}
 */
export const FormSelectOptions = {
  TITLE_OPTIONS: [
    {
      displayText: 'Short Sentence (Left-to-Right)',
      value: 'Notification Title',
    },
    {displayText: 'Short Sentence (Right-to-Left)', value: 'כותרת הודעה'},
    {
      displayText: 'Long Sentence (Left-to-Right)',
      value:
          'Hamburgers: the cornerstone of any nutritious breakfast. Ch-cheeseburgers',
    },
    {
      displayText: 'Long Sentence (Right-to-Left)',
      value: 'המבורגרים: אבן הפינה של כל ארוחת בוקר מזינה. ציזבורגר',
    },
    {
      displayText: 'Repetitive Characters (Left-to-Right)',
      value: 'sshhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh',
    },
    {
      displayText: 'Repetitive Characters (Right-to-Left)',
      value: 'שששששששששששששששששששששששששששששששששששששששששששששששששששש',
    },
    {displayText: 'Empty', value: ''},
  ],
  MESSAGE_OPTIONS: [
    {
      displayText: 'One Sentence (Left-to-Right)',
      value: 'Notification content',
    },
    {displayText: 'One Sentence (Right-to-Left)', value: 'תוכן הודעה'},
    {
      displayText: 'Multiple Sentences (Left-to-Right)',
      value:
          'This is the notification\'s message.It may be able to stretch over multiple lines, or become visible when the notification is expanded by the user, depending on the notification center that\'s being used.',
    },
    {
      displayText: 'Multiple Sentences (Right-to-Left)',
      value:
          'זהו המסר של ההודעה. זה עשוי להיות מסוגל למתוח על קווים מרובים, או להיות גלוי, כאשר ההודעה מורחבת על ידי המשתמש, בהתאם להודעה שהמרכז נמצא בשימוש',
    },
    {
      displayText: 'Repetitive Characters (Left-to-Right)',
      value:
          'sshhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh',
    },
    {
      displayText: 'Repetitive Characters (Right-to-Left)',
      value:
          'ששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששש',
    },
    {displayText: 'Unicode Emojis', value: '🌇😃🍈😆🍜🍻😋⛅⛳😚ඞ'},
    {displayText: 'Empty', value: ''},
  ],
  SMALL_IMAGE_OPTIONS: [
    {displayText: 'Default Icon', value: 'kProductIcon'},
    {displayText: 'Terminal Icon', value: 'kTerminalSshIcon'},
    {displayText: 'Credit Card Icon', value: 'kCreditCardIcon'},
    {displayText: 'Smartphone Icon', value: 'kSmartphoneIcon'},
  ],
  ICON_OPTIONS: [
    {displayText: 'No Image', value: 'none'},
    {displayText: 'Chromium Logo (PNG)', value: 'chromium_logo'},
    {displayText: 'Google Logo (PNG)', value: 'google_logo'},
    {displayText: 'Warning Symbol (PNG)', value: 'warning'},
  ],
  IMAGE_OPTIONS: [
    {displayText: 'No Image', value: 'none'},
    {
      displayText: 'Google Logo (PNG, 180 x 68)',
      value: 'google_logo_small_png',
    },
    {
      displayText: 'Chromium Logo (PNG, 192 x 192)',
      value: 'chromium_logo_large_png',
    },
  ],
  URL_OPTIONS: [
    {displayText: 'URL (Left-to-Right)', value: 'https://testurl.xyz'},
    {displayText: 'URL (Right-to-Left)', value: 'https://اختبار.النهاي'},
    {displayText: 'Empty', value: ''},
  ],
  DISPLAY_SOURCE_OPTIONS: [
    {
      displayText: 'Short Sentence (Left-to-Right)',
      value: 'Sample Display Source',
    },
    {displayText: 'Short Sentence (Right-to-Left)', value: 'مصدر عرض العينة'},
    {
      displayText: 'Long Sentence (Left-to-Right)',
      value:
          'Hamburgers: the cornerstone of any nutritious breakfast. Ch-cheeseburgers',
    },
    {
      displayText: 'Long Sentence (Right-to-Left)',
      value:
          'مصدر عرض العينةمصدر عرض العينةمصدر عرض العينةمصدر عرض العينةمصدر عرض العينةمصدر عرض العينة',
    },
    {displayText: 'Empty', value: ''},

  ],
  NOTIFICATION_TYPE_OPTIONS: [
    {displayText: 'Simple', value: NotificationType.NOTIFICATION_TYPE_SIMPLE},
    {
      displayText: 'Base Format',
      value: NotificationType.NOTIFICATION_TYPE_BASE_FORMAT,
    },
    {displayText: 'Image', value: NotificationType.NOTIFICATION_TYPE_IMAGE},
    {
      displayText: 'Multiple',
      value: NotificationType.NOTIFICATION_TYPE_MULTIPLE,
    },
    {
      displayText: 'Progress',
      value: NotificationType.NOTIFICATION_TYPE_PROGRESS,
    },
  ],
  PRIORITY_OPTIONS: [
    {displayText: 'Default', value: NotificationPriority.DEFAULT_PRIORITY},
    {displayText: 'Minimum', value: NotificationPriority.MIN_PRIORITY},
    {displayText: 'Low', value: NotificationPriority.LOW_PRIORITY},
    {displayText: 'High', value: NotificationPriority.HIGH_PRIORITY},
    {displayText: 'Max', value: NotificationPriority.MAX_PRIORITY},
    {displayText: 'System', value: NotificationPriority.SYSTEM_PRIORITY},
  ],
  PROGRESS_STATUS_OPTIONS: [
    {displayText: 'Short Sentence (Left-to-Right)', value: 'Progress Status'},
    {displayText: 'Short Sentence (Right-to-Left)', value: 'כותרת הודעה'},
    {
      displayText: 'Long Sentence (Left-to-Right)',
      value:
          'Hamburgers: the cornerstone of any nutritious breakfast. Ch-cheeseburgers',
    },
    {
      displayText: 'Long Sentence (Right-to-Left)',
      value: 'המבורגרים: אבן הפינה של כל ארוחת בוקר מזינה. ציזבורגר',
    },
    {
      displayText: 'Repetitive Characters (Left-to-Right)',
      value: 'sshhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh',
    },
    {
      displayText: 'Repetitive Characters (Right-to-Left)',
      value: 'שששששששששששששששששששששששששששששששששששששששששששששששששששש',
    },
    {displayText: 'Unicode Emojis', value: '🌇😃🍈😆🍜🍻😋⛅⛳😚ඞ'},
    {displayText: 'Empty', value: ''},
  ],
  NOTIFICATION_ID_OPTIONS: [
    {displayText: 'Random', value: 'random'},
    {displayText: 'Group A', value: 'group_a'},
    {displayText: 'Group B', value: 'group_b'},
    {displayText: 'Group C', value: 'group_c'},
  ],
  WARNING_LEVEL_OPTIONS: [
    {displayText: 'Normal', value: SystemNotificationWarningLevel.NORMAL},
    {displayText: 'Warning', value: SystemNotificationWarningLevel.WARNING},
    {
      displayText: 'Critical Warning',
      value: SystemNotificationWarningLevel.CRITICAL_WARNING,
    },
  ],
  // Value represents the # of mins prior to now to set the timestamp to.
  TIME_STAMP_OPTIONS: [
    {displayText: 'Now', value: 0},
    {displayText: '5 minutes ago', value: 5},
    {displayText: '1 hour ago', value: 60},
    {displayText: '1 day ago', value: 1440},
    {displayText: '1 week ago', value: 10080},
  ],
};

// Constant notification objects based on the notification spec:
// go/cros-notification-spec

/*
 * Text Notification (go/cros-notification-spec)
 */
const TEXT_NOTIFICATION = {
  id: 'random',
  title: 'Text',
  message: 'This is a text notification.',
  icon: 'kProductIcon',
  displaySource: 'Sample Display Source',
  originURL: '',
  notificationType: NotificationType.NOTIFICATION_TYPE_SIMPLE,
  notifierType: NotifierType.SYSTEM_COMPONENT,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'none',
  richDataSmallImage: 'none',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: -1,
  richDataProgressStatus: '',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};


/*
 * Text + Big Picture Notification (go/cros-notification-spec)
 */
const TEXT_AND_BIG_PICTURE_NOTIFICATION = {
  id: 'random',
  title: 'Text + Big Picture',
  message: 'This is a text + big picture notification.',
  icon: 'kProductIcon',
  displaySource: 'Sample Display Source',
  originURL: '',
  notificationType: NotificationType.NOTIFICATION_TYPE_SIMPLE,
  notifierType: NotifierType.SYSTEM_COMPONENT,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'chromium_logo_large_png',
  richDataSmallImage: 'none',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: -1,
  richDataProgressStatus: '',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};

/*
 * Text + Progress Notification (go/cros-notification-spec)
 */
const TEXT_AND_PROGRESS_NOTIFICATION = {
  id: 'random',
  title: 'Text + Progress',
  message: 'This is a text + progress notification.',
  icon: 'kProductIcon',
  displaySource: '',
  originURL: '',
  notificationType: NotificationType.NOTIFICATION_TYPE_PROGRESS,
  notifierType: NotifierType.SYSTEM_COMPONENT,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'none',
  richDataSmallImage: 'none',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: 50,
  richDataProgressStatus: 'Sample Progress Status',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};

/*
 * Text + Avatar (go/cros-notification-spec)
 */
const TEXT_AND_AVATAR_NOTIFICATION = {
  id: 'random',
  title: 'Text + Avatar',
  message: 'This is a text + avatar notification.',
  icon: 'chromium_logo',
  displaySource: 'Display Source',
  originURL: '',
  notificationType: NotificationType.NOTIFICATION_TYPE_SIMPLE,
  notifierType: NotifierType.SYSTEM_COMPONENT,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'none',
  richDataSmallImage: 'kProductIcon',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: -1,
  richDataProgressStatus: '',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};

/*
 * Text Group Notifications (go/cros-notification-spec)
 */
const TEXT_GROUP_NOTIFICATION_A = {
  id: 'random',
  title: 'Group - 1',
  message: 'This is the first notification.',
  icon: 'chromium_logo',
  displaySource: '',
  originURL: 'https://testurl.xyz',
  notificationType: NotificationType.NOTIFICATION_TYPE_SIMPLE,
  notifierType: NotifierType.WEB_PAGE,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'none',
  richDataSmallImage: 'kProductIcon',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: -1,
  richDataProgressStatus: '',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};
const TEXT_GROUP_NOTIFICATION_B = {
  id: 'random',
  title: 'Group - 2',
  message: 'This is the second notification.',
  icon: 'google_logo',
  displaySource: '',
  originURL: 'https://testurl.xyz',
  notificationType: NotificationType.NOTIFICATION_TYPE_SIMPLE,
  notifierType: NotifierType.WEB_PAGE,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'none',
  richDataSmallImage: 'kProductIcon',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: -1,
  richDataProgressStatus: '',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};
const TEXT_GROUP_NOTIFICATION_C = {
  id: 'random',
  title: 'Group - 3',
  message: 'This is the third notification.',
  icon: 'warning',
  displaySource: '',
  originURL: 'https://testurl.xyz',
  notificationType: NotificationType.NOTIFICATION_TYPE_SIMPLE,
  notifierType: NotifierType.WEB_PAGE,
  warningLevel: SystemNotificationWarningLevel.NORMAL,
  richDataImage: 'none',
  richDataSmallImage: 'kProductIcon',
  richDataNeverTimeout: false,
  richDataPriority: NotificationPriority.DEFAULT_PRIORITY,
  richDataPinned: false,
  richDataShowSnooze: false,
  richDataShowSettings: false,
  richDataProgress: -1,
  richDataProgressStatus: '',
  richDataNumButtons: 0,
  richDataNumNotifItems: 0,
  richDataTimestamp: 0,
};

/*
 * Array of notifications types from the spec. (go/cros-notification-spec).
 */
export const NOTIFICATION_VIEW_TYPES = [
  TEXT_NOTIFICATION,
  TEXT_AND_BIG_PICTURE_NOTIFICATION,
  TEXT_AND_PROGRESS_NOTIFICATION,
  TEXT_AND_AVATAR_NOTIFICATION,
  TEXT_GROUP_NOTIFICATION_C,
  TEXT_GROUP_NOTIFICATION_B,
  TEXT_GROUP_NOTIFICATION_A,
];
