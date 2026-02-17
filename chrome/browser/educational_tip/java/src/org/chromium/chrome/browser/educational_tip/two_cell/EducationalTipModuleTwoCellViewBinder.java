// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_1_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_CLICK_HANDLER;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_COMPLETED_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_ICON;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_MARK_COMPLETED;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.ITEM_2_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.MODULE_TITLE;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipModuleTwoCellProperties.SEE_MORE_CLICK_HANDLER;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for a generic two-cell educational tip module. Binds data from the PropertyModel to
 * the EducationalTipModuleTwoCellView.
 */
@NullMarked
public class EducationalTipModuleTwoCellViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        EducationalTipModuleTwoCellView moduleView = (EducationalTipModuleTwoCellView) view;

        if (MODULE_TITLE == propertyKey) {
            moduleView.setModuleTitle(model.get(MODULE_TITLE));
        }
        if (SEE_MORE_CLICK_HANDLER == propertyKey) {
            moduleView.setSeeMoreOnClickListener(v -> model.get(SEE_MORE_CLICK_HANDLER).run());
        } else if (ITEM_1_TITLE == propertyKey) {
            moduleView.setItem1Title(model.get(ITEM_1_TITLE));
        } else if (ITEM_1_DESCRIPTION == propertyKey) {
            moduleView.setItem1Description(model.get(ITEM_1_DESCRIPTION));
        } else if (ITEM_1_ICON == propertyKey) {
            moduleView.setItem1Icon(model.get(ITEM_1_ICON));
        } else if (ITEM_1_COMPLETED_ICON == propertyKey) {
            moduleView.setItem1IconWithAnimation(model.get(ITEM_1_COMPLETED_ICON));
        } else if (ITEM_1_CLICK_HANDLER == propertyKey) {
            moduleView.setItem1OnClickListener(v -> model.get(ITEM_1_CLICK_HANDLER).run());
        } else if (ITEM_1_MARK_COMPLETED == propertyKey) {
            moduleView.setItem1Completed(model.get(ITEM_1_MARK_COMPLETED));
        } else if (ITEM_2_TITLE == propertyKey) {
            moduleView.setItem2Title(model.get(ITEM_2_TITLE));
        } else if (ITEM_2_DESCRIPTION == propertyKey) {
            moduleView.setItem2Description(model.get(ITEM_2_DESCRIPTION));
        } else if (ITEM_2_ICON == propertyKey) {
            moduleView.setItem2Icon(model.get(ITEM_2_ICON));
        } else if (ITEM_2_COMPLETED_ICON == propertyKey) {
            moduleView.setItem2IconWithAnimation(model.get(ITEM_2_COMPLETED_ICON));
        } else if (ITEM_2_CLICK_HANDLER == propertyKey) {
            moduleView.setItem2OnClickListener(v -> model.get(ITEM_2_CLICK_HANDLER).run());
        } else if (ITEM_2_MARK_COMPLETED == propertyKey) {
            moduleView.setItem2Completed(model.get(ITEM_2_MARK_COMPLETED));
        }
    }
}
