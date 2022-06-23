// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * @fileoverview Stores the content of the notification tester form select
 * elements.
 * @externs
 */

/*
 * Stores the content of the notification tester form select
 * elements. Options correspond to the naming scheme defined in
 * ui/message_center/public/cpp/notification.h.
 * @enum {Array}
 */
export const FormSelectOptions = {
  TITLE_OPTIONS: [
    {displayText: 'Short Sentence (LTR)', value: 'Notification Title'},
    {displayText: 'Short Sentence (RTL)', value: 'כותרת הודעה'}, {
      displayText: 'Long Sentence (LTR)',
      value:
          'Hamburgers: the cornerstone of any nutritious breakfast. Ch-cheeseburgers'
    },
    {
      displayText: 'Long Sentence (RTL)',
      value: 'המבורגרים: אבן הפינה של כל ארוחת בוקר מזינה. ציזבורגר'
    },
    {
      displayText: 'Repetitive Characters (LTR)',
      value: 'sshhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh'
    },
    {
      displayText: 'Repetitive Characters (RTL)',
      value: 'שששששששששששששששששששששששששששששששששששששששששששששששששששש'
    }
  ],
  MESSAGE_OPTIONS: [
    {displayText: 'One Sentence (LTR)', value: 'Notification content'},
    {displayText: 'One Sentence (RTL)', value: 'תוכן הודעה'},
    {
      displayText: 'Multiple Sentences (LTR)',
      value:
          'This is the notification\'s message.It may be able to stretch over multiple lines, or become visible when the notification is expanded by the user, depending on the notification center that\'s being used.'
    },
    {
      displayText: 'Multiple Sentences (RTL)',
      value:
          'זהו המסר של ההודעה. זה עשוי להיות מסוגל למתוח על קווים מרובים, או להיות גלוי, כאשר ההודעה מורחבת על ידי המשתמש, בהתאם להודעה שהמרכז נמצא בשימוש'
    },
    {
      displayText: 'Repetitive Characters (LTR)',
      value:
          'sshhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh'
    },
    {
      displayText: 'Repetitive Characters (RTL)',
      value:
          'ששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששששש'
    },
    {displayText: 'Unicode Emojis', value: '🌇😃🍈😆🍜🍻😋⛅⛳😚ඞ'},
    {displayText: 'Empty', value: ''},
  ],
  BADGE_OPTIONS: [
    {displayText: 'Cat Badge', value: 'cat'},
    {displayText: 'No Badge', value: 'none'}
  ],
  IMAGE_OPTIONS: [
    {displayText: 'No Image', value: 'none'},
    {displayText: 'Cat Image (JPEG, 3072x1728)', value: 'cat'},
  ],
  ICON_OPTIONS: [
    {displayText: 'No Icon', value: 'none'},
    {displayText: 'Normal PNG Icon (256x256)', value: 'normal_png'},
    {displayText: 'Static GIF Icon (256x256)', value: 'static_gif'},
    {displayText: 'Animated GIF Icon (256x256)', value: 'animated_gif'},
    {displayText: 'Tiny Icon (PNG, 32x32)', value: 'tiny_icon'},
    {displayText: 'Huge Icon (PNG, 3333x5230)', value: 'huge_icon'},
  ],
};