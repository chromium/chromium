// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.ContextThemeWrapper;
import android.view.View;
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

    @Mock private View mView;
    @Mock private TextView mTitleView;
    @Mock private TextView mShortcutView;
    @Mock private ImageView mIconView;
    @Mock private TextView mTextView;
    @Mock private ImageView mActionIconView;
    @Mock private ListMenuButton mMenuButtonView;
    @Mock private Bitmap mBitmap;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private ListMenuDelegate mMenuDelegate;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private SearchEngineListPreference mPreference;

    private Context mContext;
    private PropertyModel mModel;
    private SiteSearchViewBinder.ViewHolder mViewHolder;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        when(mView.findViewById(R.id.name)).thenReturn(mTitleView);
        when(mView.findViewById(R.id.shortcut)).thenReturn(mShortcutView);
        when(mView.findViewById(R.id.favicon)).thenReturn(mIconView);
        when(mView.findViewById(R.id.text)).thenReturn(mTextView);
        when(mView.findViewById(R.id.action_icon)).thenReturn(mActionIconView);
        when(mView.findViewById(R.id.overflow_menu_button)).thenReturn(mMenuButtonView);
        when(mView.getContext()).thenReturn(mContext);

        mViewHolder = new SiteSearchViewBinder.ViewHolder(mView);
        when(mView.getTag()).thenReturn(mViewHolder);

        mModel = new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS).build();
    }

    @Test
    public void testBindSiteName() {
        String siteName = "Test";
        mModel.set(SiteSearchProperties.SITE_NAME, siteName);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.SITE_NAME);

        verify(mTitleView).setText(siteName);
    }

    @Test
    public void testBindSiteShortcut() {
        String shortcut = "test";
        mModel.set(SiteSearchProperties.SITE_SHORTCUT, shortcut);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.SITE_SHORTCUT);

        verify(mShortcutView).setText(shortcut);
    }

    @Test
    public void testBindIcon() {
        mModel.set(SiteSearchProperties.ICON, mBitmap);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.ICON);

        verify(mIconView).setImageBitmap(mBitmap);
    }

    @Test
    public void testBindOnClickListener() {
        mModel.set(SiteSearchProperties.ON_CLICK, mOnClickListener);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.ON_CLICK);

        verify(mView).setOnClickListener(mOnClickListener);
    }

    @Test
    public void testText() {
        String buttonText = "More";
        mModel.set(SiteSearchProperties.TEXT, buttonText);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.TEXT);

        verify(mTextView).setText(buttonText);
    }

    @Test
    public void testBindIsExpanded_True() {
        mModel.set(SiteSearchProperties.IS_EXPANDED, true);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.IS_EXPANDED);

        verify(mActionIconView).setImageResource(R.drawable.ic_expand_less_black_24dp);
    }

    @Test
    public void testBindIsExpanded_False() {
        mModel.set(SiteSearchProperties.IS_EXPANDED, false);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.IS_EXPANDED);

        verify(mActionIconView).setImageResource(R.drawable.ic_expand_more_black_24dp);
    }

    @Test
    public void testBindMenuDelegate_NotNull() {
        mModel.set(SiteSearchProperties.MENU_DELEGATE, mMenuDelegate);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.MENU_DELEGATE);

        verify(mMenuButtonView).setDelegate(mMenuDelegate);
        verify(mMenuButtonView).setEnabled(true);
    }

    @Test
    public void testBindMenuDelegate_Null() {
        mModel.set(SiteSearchProperties.MENU_DELEGATE, null);
        SiteSearchViewBinder.bind(mModel, mView, SiteSearchProperties.MENU_DELEGATE);

        verify(mMenuButtonView).setDelegate(null);
        verify(mMenuButtonView).setEnabled(false);
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
        View realView = new View(mContext);
        realView.setTag(mViewHolder);

        mModel.set(SiteSearchProperties.IS_EXPANDED, true);
        SiteSearchViewBinder.bind(mModel, realView, SiteSearchProperties.IS_EXPANDED);

        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(realView);
        assertNotNull(delegate);

        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        delegate.onInitializeAccessibilityNodeInfo(realView, info);

        assertEquals(AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL, info.getExpandedState());
        List<AccessibilityActionCompat> actionList = info.getActionList();
        assertTrue(actionList.contains(AccessibilityActionCompat.ACTION_COLLAPSE));

        AtomicBoolean clicked = new AtomicBoolean(false);
        realView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                delegate.performAccessibilityAction(
                        realView, AccessibilityActionCompat.ACTION_COLLAPSE.getId(), null);
        assertTrue(handled);
        assertTrue(clicked.get());
    }

    @Test
    public void testBindIsExpanded_False_Accessibility() {
        View realView = new View(mContext);
        realView.setTag(mViewHolder);

        mModel.set(SiteSearchProperties.IS_EXPANDED, false);
        SiteSearchViewBinder.bind(mModel, realView, SiteSearchProperties.IS_EXPANDED);

        AccessibilityDelegateCompat delegate = ViewCompat.getAccessibilityDelegate(realView);
        assertNotNull(delegate);

        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        delegate.onInitializeAccessibilityNodeInfo(realView, info);

        assertEquals(AccessibilityNodeInfoCompat.EXPANDED_STATE_COLLAPSED, info.getExpandedState());
        List<AccessibilityActionCompat> actionList = info.getActionList();
        assertTrue(actionList.contains(AccessibilityActionCompat.ACTION_EXPAND));

        AtomicBoolean clicked = new AtomicBoolean(false);
        realView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                delegate.performAccessibilityAction(
                        realView, AccessibilityActionCompat.ACTION_EXPAND.getId(), null);
        assertTrue(handled);
        assertTrue(clicked.get());
    }
}
