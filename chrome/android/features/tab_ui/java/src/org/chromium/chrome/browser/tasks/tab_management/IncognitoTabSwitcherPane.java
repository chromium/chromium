// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/** A {@link Pane} representing the incognito tab switcher. */
public class IncognitoTabSwitcherPane extends TabSwitcherPaneBase {
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void wasFirstTabCreated() {
                    mReferenceButtonDataSupplier.set(mReferenceButtonData);
                }

                @Override
                public void didBecomeEmpty() {
                    mReferenceButtonDataSupplier.set(null);
                }
            };

    /** Not safe to use until initWithNative. */
    private final @NonNull Supplier<IncognitoTabModel> mIncognitoTabModelSupplier;

    private final @NonNull ResourceButtonData mReferenceButtonData;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param incognitoTabModelSupplier The supplier of the incognito tab model. Returns a valid
     *     model once native is loaded and will never change thereafter.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param menuOrKeyboardActionController allows access to menu or keyboard actions.
     */
    IncognitoTabSwitcherPane(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull Supplier<IncognitoTabModel> incognitoTabModelSupplier,
            @NonNull OnClickListener newTabButtonClickListener,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController) {
        super(
                context,
                factory,
                newTabButtonClickListener,
                menuOrKeyboardActionController,
                org.chromium.chrome.browser.toolbar.R.string.button_new_incognito_tab);

        mIncognitoTabModelSupplier = incognitoTabModelSupplier;

        // TODO(crbug/1505772): Update this string to not be an a11y string and it should probably
        // just say "Incognito".
        mReferenceButtonData =
                new ResourceButtonData(
                        R.string.accessibility_tab_switcher,
                        R.string.accessibility_tab_switcher,
                        R.drawable.incognito_small);
    }

    @Override
    public void destroy() {
        super.destroy();
        IncognitoTabModel incognitoTabModel = mIncognitoTabModelSupplier.get();
        if (incognitoTabModel != null) {
            incognitoTabModel.removeIncognitoObserver(mIncognitoTabModelObserver);
        }
    }

    @Override
    public void initWithNative() {
        super.initWithNative();
        IncognitoTabModel incognitoTabModel = mIncognitoTabModelSupplier.get();
        incognitoTabModel.addIncognitoObserver(mIncognitoTabModelObserver);
        if (incognitoTabModel.getCount() > 0) {
            mIncognitoTabModelObserver.wasFirstTabCreated();
        }
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.INCOGNITO_TAB_SWITCHER;
    }
}
