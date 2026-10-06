// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.browser_ui.widget.containment.ContainerStyle;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link SiteSearchViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SiteSearchViewBinderUnitTest {
    private static final float TOLERANCE = 0.001f;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnClickListener;
    @Mock private ListMenuDelegate mMenuDelegate;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private SearchEngineListPreference mPreference;

    private Context mContext;
    private FrameLayout mView;
    private TextView mTitleView;
    private TextView mShortcutView;
    private ImageView mIconView;
    private TextView mTextView;
    private ImageView mActionIconView;
    private ListMenuButton mMenuButtonView;
    private PropertyModel mModel;
    private SiteSearchViewBinder.ViewHolder mViewHolder;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mView = new FrameLayout(mContext);
        mTitleView = addChild(new TextView(mContext), R.id.name);
        mShortcutView = addChild(new TextView(mContext), R.id.shortcut);
        mIconView = addChild(new ImageView(mContext), R.id.favicon);
        mTextView = addChild(new TextView(mContext), R.id.text);
        mActionIconView = addChild(new ImageView(mContext), R.id.action_icon);
        mMenuButtonView = addChild(new ListMenuButton(mContext, null), R.id.overflow_menu_button);

        mViewHolder = new SiteSearchViewBinder.ViewHolder(mView);
        mView.setTag(mViewHolder);

        mModel = new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS).build();
    }

    private <T extends View> T addChild(T child, int id) {
        child.setId(id);
        mView.addView(child);
        return child;
    }

    @Test
    public void testBindSiteName() {
        String siteName = "Test";
        mModel.set(SiteSearchProperties.SITE_NAME, siteName);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.SITE_NAME);

        assertEquals(siteName, mTitleView.getText().toString());
    }

    @Test
    public void testBindSiteShortcut() {
        String shortcut = "test";
        mModel.set(SiteSearchProperties.SITE_SHORTCUT, shortcut);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.SITE_SHORTCUT);

        assertEquals(shortcut, mShortcutView.getText().toString());
    }

    @Test
    public void testBindIcon() {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mModel.set(SiteSearchProperties.ICON, bitmap);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.ICON);

        assertEquals(bitmap, ((BitmapDrawable) mIconView.getDrawable()).getBitmap());
    }

    @Test
    public void testBindOnClickListener() {
        mModel.set(SiteSearchProperties.ON_CLICK, mOnClickListener);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.ON_CLICK);

        mView.performClick();
        verify(mOnClickListener).onClick(mView);
    }

    @Test
    public void testText() {
        String buttonText = "More";
        mModel.set(SiteSearchProperties.TEXT, buttonText);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.TEXT);

        assertEquals(buttonText, mTextView.getText().toString());
    }

    @Test
    public void testBindIsExpanded_True() {
        mModel.set(SiteSearchProperties.IS_EXPANDED, true);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.IS_EXPANDED);

        assertEquals(
                R.drawable.ic_expand_less_black_24dp,
                shadowOf(mActionIconView.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testBindIsExpanded_False() {
        mModel.set(SiteSearchProperties.IS_EXPANDED, false);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.IS_EXPANDED);

        assertEquals(
                R.drawable.ic_expand_more_black_24dp,
                shadowOf(mActionIconView.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testBindMenuDelegate_NotNull() {
        mMenuButtonView.setEnabled(false);
        mModel.set(SiteSearchProperties.MENU_DELEGATE, mMenuDelegate);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.MENU_DELEGATE);

        // setDelegate() installs a click listener that shows the menu.
        assertTrue(mMenuButtonView.hasOnClickListeners());
        assertTrue(mMenuButtonView.isEnabled());
    }

    @Test
    public void testBindMenuDelegate_Null() {
        mModel.set(SiteSearchProperties.MENU_DELEGATE, null);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.MENU_DELEGATE);

        assertFalse(mMenuButtonView.isEnabled());
    }

    @Test
    public void testBindPreference_Adapter() {
        mModel.set(SiteSearchProperties.ADAPTER, mAdapter);
        SiteSearchViewBinder.bindPreference(mModel, mPreference, SiteSearchProperties.ADAPTER);

        verify(mPreference).setAdapter(mAdapter);
    }

    @Test
    public void testCreateBackgroundStyle_Top() {
        ContainmentItemController controller = new ContainmentItemController(mContext);
        ContainerStyle style =
                SiteSearchViewBinder.createBackgroundStyle(
                        controller, SiteSearchProperties.ItemPosition.TOP);

        assertNotNull(style);
        assertEquals(0f, style.getBottomRadius(), TOLERANCE);
        assertTrue(style.getTopRadius() > 0f);
    }

    @Test
    public void testCreateBackgroundStyle_Bottom() {
        ContainmentItemController controller = new ContainmentItemController(mContext);
        ContainerStyle style =
                SiteSearchViewBinder.createBackgroundStyle(
                        controller, SiteSearchProperties.ItemPosition.BOTTOM);

        assertNotNull(style);
        assertEquals(0f, style.getTopRadius(), TOLERANCE);
        assertTrue(style.getBottomRadius() > 0f);
    }

    @Test
    public void testCreateBackgroundStyle_Middle() {
        ContainmentItemController controller = new ContainmentItemController(mContext);
        ContainerStyle style =
                SiteSearchViewBinder.createBackgroundStyle(
                        controller, SiteSearchProperties.ItemPosition.MIDDLE);

        assertNotNull(style);
        assertEquals(0f, style.getTopRadius(), TOLERANCE);
        assertEquals(0f, style.getBottomRadius(), TOLERANCE);
    }

    @Test
    public void testCreateBackgroundStyle_Single() {
        ContainmentItemController controller = new ContainmentItemController(mContext);
        ContainerStyle style =
                SiteSearchViewBinder.createBackgroundStyle(
                        controller, SiteSearchProperties.ItemPosition.SINGLE);

        assertNotNull(style);
        assertTrue(style.getTopRadius() > 0f);
        assertTrue(style.getBottomRadius() > 0f);
    }

    @Test
    public void testBindIsExpanded_True_Accessibility() {
        mModel.set(SiteSearchProperties.IS_EXPANDED, true);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.IS_EXPANDED);

        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(mView);
        assertNotNull(delegate);

        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        delegate.onInitializeAccessibilityNodeInfo(mView, info);

        assertEquals(AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL, info.getExpandedState());
        List<AccessibilityActionCompat> actionList = info.getActionList();
        assertTrue(actionList.contains(AccessibilityActionCompat.ACTION_COLLAPSE));

        AtomicBoolean clicked = new AtomicBoolean(false);
        mView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                delegate.performAccessibilityAction(
                        mView, AccessibilityActionCompat.ACTION_COLLAPSE.getId(), null);
        assertTrue(handled);
        assertTrue(clicked.get());
    }

    @Test
    public void testBindIsExpanded_False_Accessibility() {
        mModel.set(SiteSearchProperties.IS_EXPANDED, false);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.IS_EXPANDED);

        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(mView);
        assertNotNull(delegate);

        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        delegate.onInitializeAccessibilityNodeInfo(mView, info);

        assertEquals(AccessibilityNodeInfoCompat.EXPANDED_STATE_COLLAPSED, info.getExpandedState());
        List<AccessibilityActionCompat> actionList = info.getActionList();
        assertTrue(actionList.contains(AccessibilityActionCompat.ACTION_EXPAND));

        AtomicBoolean clicked = new AtomicBoolean(false);
        mView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                delegate.performAccessibilityAction(
                        mView, AccessibilityActionCompat.ACTION_EXPAND.getId(), null);
        assertTrue(handled);
        assertTrue(clicked.get());
    }
}
