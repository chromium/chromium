// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ActionButton;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link DownloadTrayItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadTrayItemViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnOpen;
    @Mock private Callback<ContentId> mOnPrimaryAction;
    @Mock private Callback<ContentId> mOnSecondaryAction;

    private View mView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = LayoutInflater.from(activity).inflate(R.layout.download_tray_item_row, null);
    }

    @Test
    public void testBind_showsRowAndRoutesClicks() {
        PropertyModel model =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.ICON, R.drawable.ic_drive_image_24dp)
                        .with(DownloadTrayItemProperties.TITLE, "report-final.pdf")
                        .with(DownloadTrayItemProperties.DESCRIPTION, "4.30 MB • Done")
                        .with(DownloadTrayItemProperties.ON_OPEN, mOnOpen)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, DownloadTrayItemViewBinder::bind);

        ImageView icon = mView.findViewById(R.id.download_tray_item_icon);
        assertEquals(
                R.drawable.ic_drive_image_24dp, shadowOf(icon.getDrawable()).getCreatedFromResId());
        assertEquals(
                "report-final.pdf",
                mView.<TextView>findViewById(R.id.download_tray_item_title).getText().toString());
        assertEquals(
                "4.30 MB • Done",
                mView.<TextView>findViewById(R.id.download_tray_item_description)
                        .getText()
                        .toString());

        // Clicking the row opens the download, and TalkBack announces it as "Open".
        mView.performClick();
        verify(mOnOpen).onClick(mView);
        AccessibilityNodeInfoCompat rowInfo =
                AccessibilityNodeInfoCompat.wrap(mView.createAccessibilityNodeInfo());
        String openLabel = mView.getContext().getString(R.string.open_downloaded_label);
        assertTrue(
                rowInfo.getActionList().stream()
                        .anyMatch(
                                action ->
                                        action.getId() == AccessibilityNodeInfoCompat.ACTION_CLICK
                                                && openLabel.equals(
                                                        String.valueOf(action.getLabel()))));

        // Rows update in place, e.g. after a rename.
        model.set(DownloadTrayItemProperties.TITLE, "renamed.pdf");
        assertEquals(
                "renamed.pdf",
                mView.<TextView>findViewById(R.id.download_tray_item_title).getText().toString());
    }

    @Test
    public void testBind_rowWithNothingToOpen_isNotClickableOrFocusable() {
        PropertyModel model =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.ON_OPEN, mOnOpen)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, DownloadTrayItemViewBinder::bind);

        // E.g. a recycled row now showing an interrupted download, which has no file to open.
        model.set(DownloadTrayItemProperties.ON_OPEN, null);
        assertFalse(mView.isClickable());
        assertFalse(mView.isFocusable());
        assertTrue(mView.isScreenReaderFocusable());
        AccessibilityNodeInfoCompat rowInfo =
                AccessibilityNodeInfoCompat.wrap(mView.createAccessibilityNodeInfo());
        assertFalse(
                rowInfo.getActionList().stream()
                        .anyMatch(
                                action ->
                                        action.getId()
                                                == AccessibilityNodeInfoCompat.ACTION_CLICK));
    }

    @Test
    public void testBind_showsProgressBarOnlyWhileInProgress() {
        ProgressBar progress = mView.findViewById(R.id.download_tray_item_progress);
        assertEquals(View.GONE, progress.getVisibility());
        PropertyModel model =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.IS_IN_PROGRESS, true)
                        .with(DownloadTrayItemProperties.PROGRESS, 40)
                        .with(DownloadTrayItemProperties.IS_PAUSED, false)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, DownloadTrayItemViewBinder::bind);

        assertEquals(View.VISIBLE, progress.getVisibility());
        assertEquals(40, progress.getProgress());
        assertFalse(progress.isIndeterminate());
        Context context = mView.getContext();
        ColorStateList activeTint =
                ColorStateList.valueOf(SemanticColorUtils.getProgressBarForeground(context));
        assertEquals(activeTint, progress.getProgressTintList());

        // Paused, the bar loses its accent colour; with no known size it animates.
        model.set(DownloadTrayItemProperties.IS_PAUSED, true);
        ColorStateList pausedTint =
                ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColorSecondary(context));
        assertNotEquals(activeTint, pausedTint);
        assertEquals(pausedTint, progress.getProgressTintList());
        assertEquals(pausedTint, progress.getIndeterminateTintList());
        model.set(
                DownloadTrayItemProperties.PROGRESS,
                DownloadTrayItemProperties.PROGRESS_INDETERMINATE);
        assertTrue(progress.isIndeterminate());
        // Once the size is known, it fills again; once the download ends, the bar goes.
        model.set(DownloadTrayItemProperties.PROGRESS, 60);
        assertFalse(progress.isIndeterminate());
        assertEquals(60, progress.getProgress());
        model.set(DownloadTrayItemProperties.IS_IN_PROGRESS, false);
        assertEquals(View.GONE, progress.getVisibility());
    }

    /** Returns the label of the row's accessibility action {@code actionId}, or null if none. */
    private String getAccessibilityActionLabel(int actionId) {
        AccessibilityNodeInfoCompat rowInfo =
                AccessibilityNodeInfoCompat.wrap(mView.createAccessibilityNodeInfo());
        return rowInfo.getActionList().stream()
                .filter(action -> action.getId() == actionId)
                .map(action -> String.valueOf(action.getLabel()))
                .findFirst()
                .orElse(null);
    }

    @Test
    public void testBind_actionButtonsAreAlsoRowAccessibilityActions() {
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        // In a window, after another focusable view, for focus to behave as in the tray.
        Activity activity = (Activity) mView.getContext();
        FrameLayout root = new FrameLayout(activity);
        View firstView = new View(activity);
        firstView.setFocusable(true);
        root.addView(firstView);
        root.addView(mView);
        activity.setContentView(root);
        ContentId itemId = LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, "a");
        ActionButton pauseAction =
                new ActionButton(
                        R.drawable.ic_pause_white_24dp,
                        R.string.download_notification_pause_button,
                        R.string.download_tray_item_pause_accessibility,
                        mOnPrimaryAction);
        ActionButton cancelAction =
                new ActionButton(
                        R.drawable.material_ic_close_24dp,
                        R.string.download_notification_cancel_button,
                        R.string.download_tray_item_cancel_accessibility,
                        mOnSecondaryAction);
        PropertyModel model =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.ITEM_ID, itemId)
                        .with(DownloadTrayItemProperties.TITLE, "report.pdf")
                        .with(DownloadTrayItemProperties.ON_OPEN, null)
                        .with(DownloadTrayItemProperties.PRIMARY_ACTION, pauseAction)
                        .with(DownloadTrayItemProperties.SECONDARY_ACTION, cancelAction)
                        .build();
        PropertyModelChangeProcessor.create(model, mView, DownloadTrayItemViewBinder::bind);
        ImageView primary = mView.findViewById(R.id.download_tray_item_primary_action);
        ImageView secondary = mView.findViewById(R.id.download_tray_item_secondary_action);
        String pause = activity.getString(R.string.download_notification_pause_button);
        String cancel = activity.getString(R.string.download_notification_cancel_button);

        // Each action has its button and, for screen readers, which can't reach a hidden button,
        // an accessibility action on the row, both labelled with the title to tell rows apart.
        assertEquals(
                R.drawable.ic_pause_white_24dp,
                shadowOf(primary.getDrawable()).getCreatedFromResId());
        assertEquals(
                R.drawable.material_ic_close_24dp,
                shadowOf(secondary.getDrawable()).getCreatedFromResId());
        assertEquals("Pause report.pdf", primary.getContentDescription().toString());
        assertEquals("Cancel report.pdf", secondary.getContentDescription().toString());
        assertEquals("Pause report.pdf", getAccessibilityActionLabel(primary.getId()));
        assertEquals("Cancel report.pdf", getAccessibilityActionLabel(secondary.getId()));

        // The labels follow a rename; with no title, they are the verb alone.
        model.set(DownloadTrayItemProperties.TITLE, "renamed.pdf");
        assertEquals("Pause renamed.pdf", primary.getContentDescription().toString());
        assertEquals("Cancel renamed.pdf", getAccessibilityActionLabel(secondary.getId()));
        model.set(DownloadTrayItemProperties.TITLE, "");
        assertEquals(pause, primary.getContentDescription().toString());
        assertEquals(cancel, getAccessibilityActionLabel(secondary.getId()));
        primary.performClick();
        verify(mOnPrimaryAction).onResult(itemId);
        assertTrue(mView.performAccessibilityAction(primary.getId(), null));
        verify(mOnPrimaryAction, times(2)).onResult(itemId);
        secondary.performClick();
        verify(mOnSecondaryAction).onResult(itemId);
        assertTrue(mView.performAccessibilityAction(secondary.getId(), null));
        verify(mOnSecondaryAction, times(2)).onResult(itemId);

        // Though it has nothing to open, the row takes keyboard focus, which shows its buttons.
        assertFalse(mView.isClickable());
        assertTrue(mView.requestFocus());
        View actions = mView.findViewById(R.id.download_tray_item_actions);
        assertEquals(View.VISIBLE, actions.getVisibility());
        assertTrue(primary.requestFocus());

        // An action that goes takes its button and accessibility action with it, and focus on
        // the button moves to the row rather than jump to the first view.
        model.set(DownloadTrayItemProperties.PRIMARY_ACTION, null);
        assertEquals(View.GONE, primary.getVisibility());
        assertNull(getAccessibilityActionLabel(primary.getId()));
        assertTrue(mView.isFocused());
        assertTrue(mView.isFocusable());

        // Without its last action the row no longer takes focus. Reused, it shows the button
        // again.
        model.set(DownloadTrayItemProperties.SECONDARY_ACTION, null);
        assertFalse(mView.isFocusable());
        model.set(DownloadTrayItemProperties.PRIMARY_ACTION, pauseAction);
        assertEquals(View.VISIBLE, primary.getVisibility());
        assertEquals(pause, getAccessibilityActionLabel(primary.getId()));
    }

    @Test
    public void testActionButton_equalValuesDoNotRebind() {
        ContentId itemId = LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, "a");
        ActionButton pauseAction =
                new ActionButton(
                        R.drawable.ic_pause_white_24dp,
                        R.string.download_notification_pause_button,
                        R.string.download_tray_item_pause_accessibility,
                        mOnPrimaryAction);
        ActionButton equalPauseAction =
                new ActionButton(
                        R.drawable.ic_pause_white_24dp,
                        R.string.download_notification_pause_button,
                        R.string.download_tray_item_pause_accessibility,
                        mOnPrimaryAction);
        assertEquals(pauseAction, equalPauseAction);
        assertEquals(pauseAction.hashCode(), equalPauseAction.hashCode());
        assertNotEquals(
                pauseAction,
                new ActionButton(
                        R.drawable.ic_play_arrow_white_24dp,
                        R.string.download_notification_resume_button,
                        R.string.download_tray_item_resume_accessibility,
                        mOnPrimaryAction));
        assertNotEquals(
                pauseAction,
                new ActionButton(
                        R.drawable.ic_pause_white_24dp,
                        R.string.download_notification_pause_button,
                        R.string.download_tray_item_pause_accessibility,
                        mOnSecondaryAction));

        PropertyModel model =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.ITEM_ID, itemId)
                        .with(DownloadTrayItemProperties.PRIMARY_ACTION, pauseAction)
                        .build();
        List<PropertyKey> changedKeys = new ArrayList<>();
        model.addObserver((_, key) -> changedKeys.add(key));
        model.set(DownloadTrayItemProperties.PRIMARY_ACTION, equalPauseAction);
        assertTrue(changedKeys.isEmpty());
    }
}
