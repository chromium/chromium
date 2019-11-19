// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Supervision Transition screen implementation.
 */

login.createScreen(
    'SupervisionTransitionScreen', 'supervision-transition', function() {
      return {
        /** @override */
        get defaultControl() {
          return $('supervision-transition-md');
        },

        /** @override */
        onBeforeShow: function(data) {
          $('supervision-transition-md')
              .setIsRemovingSupervision(
                  data['isRemovingSupervision'] ? true : false);
        },
      };
    });
