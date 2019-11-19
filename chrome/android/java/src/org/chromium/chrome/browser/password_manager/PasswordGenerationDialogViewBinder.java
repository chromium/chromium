// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/** Class responsible for binding the model and the view. On bind, it lazily initializes the view
 * since all the needed data was made available at this point.
 */
public class PasswordGenerationDialogViewBinder {
    public static void bind(
            PasswordGenerationDialogModel model, PasswordGenerationDialogCustomView viewHolder) {
        viewHolder.setGeneratedPassword(
                model.get(PasswordGenerationDialogModel.GENERATED_PASSWORD));
        viewHolder.setSaveExplanationText(
                model.get(PasswordGenerationDialogModel.SAVE_EXPLANATION_TEXT));
    }
}
