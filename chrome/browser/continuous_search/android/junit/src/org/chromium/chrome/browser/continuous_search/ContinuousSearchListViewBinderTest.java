// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemProperties;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Test of {@link ContinuousSearchContainerViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContinuousSearchListViewBinderTest {
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    private Activity mActivity;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
    }

    @Test
    public void testBindListItem() {
        View view = mock(View.class);
        ContinuousSearchChipView chipView = mock(ContinuousSearchChipView.class);
        TextView textView = mock(TextView.class);
        InOrder inOrder = inOrder(view, chipView, textView);
        PropertyModel model = new PropertyModel(ListItemProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                model, view, ContinuousSearchListViewBinder::bindListItem);

        Resources resources = mock(Resources.class);
        doReturn(chipView).when(view).findViewById(anyInt());
        doReturn(resources).when(view).getResources();

        final int maxWidth = 20;
        final String label = "Label";
        final String anotherLabel = "AnotherLabel";
        final GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final GURL anotherUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        doReturn(maxWidth).when(resources).getDimensionPixelSize(anyInt());
        when(chipView.getPrimaryTextView()).thenReturn(textView);
        when(chipView.getSecondaryTextView()).thenReturn(textView);

        // Test label and url setup based on whether we should show result titles.
        when(chipView.isTwoLineChipView()).thenReturn(false);
        model.set(ListItemProperties.LABEL, label);
        inOrder.verify(chipView, times(0)).getPrimaryTextView();
        model.set(ListItemProperties.URL, url);
        inOrder.verify(chipView).getPrimaryTextView();
        when(chipView.isTwoLineChipView()).thenReturn(true);
        model.set(ListItemProperties.LABEL, anotherLabel);
        inOrder.verify(chipView).getPrimaryTextView();
        model.set(ListItemProperties.URL, anotherUrl);
        inOrder.verify(chipView).getSecondaryTextView();

        model.set(ListItemProperties.LABEL, label);
        checkTextViewSetup(inOrder, textView, label, maxWidth, TruncateAt.END);

        final String safeUrl = "example.com";
        doReturn(safeUrl)
                .when(mUrlFormatterJniMock)
                .formatUrlForSecurityDisplay(eq(url), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS));
        model.set(ListItemProperties.URL, url);
        checkTextViewSetup(inOrder, textView, safeUrl, maxWidth, TruncateAt.START);

        int backgroundColor = 0xCCBBAA;
        model.set(ListItemProperties.BACKGROUND_COLOR, backgroundColor);
        inOrder.verify(chipView).setBackgroundColor(backgroundColor);

        int borderWidth = 10;
        int borderColor = 0xAABBCC;
        when(chipView.getResources()).thenReturn(resources);
        doReturn(borderWidth).when(resources).getDimensionPixelSize(anyInt());
        model.set(ListItemProperties.BORDER_COLOR, borderColor);
        inOrder.verify(chipView).setBorder(borderWidth, backgroundColor);
        model.set(ListItemProperties.IS_SELECTED, true);
        inOrder.verify(chipView).setBorder(borderWidth, borderColor);

        View.OnClickListener listener = (v) -> {};
        model.set(ListItemProperties.CLICK_LISTENER, listener);
        inOrder.verify(view).setOnClickListener(eq(listener));

        int id = 90;
        model.set(ListItemProperties.PRIMARY_TEXT_STYLE, id);
        inOrder.verify(textView).setTextAppearance(any(), eq(id));
    }

    private void checkTextViewSetup(InOrder inOrder, TextView textView, String text, int maxWidth,
            TextUtils.TruncateAt truncateAt) {
        inOrder.verify(textView).setSingleLine();
        inOrder.verify(textView).setMaxLines(1);
        inOrder.verify(textView).setEllipsize(truncateAt);
        inOrder.verify(textView).setTextDirection(View.TEXT_DIRECTION_LTR);
        inOrder.verify(textView).setMaxWidth(maxWidth);
        inOrder.verify(textView).setText(text);
    }

    /**
     * Verifies that a really long domain name will have the correct ellipsis behavior. This uses
     * the real layout XML in order to ensure it is configured correctly.
     */
    @Test
    public void testEllipsizeLongDomain() {
        View view = mock(View.class);
        ChipView chipView = (ChipView) LayoutInflater.from(mActivity).inflate(
                R.layout.continuous_search_list_item, null);
        TextView textView = chipView.getPrimaryTextView();
        PropertyModel model = new PropertyModel(ListItemProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                model, view, ContinuousSearchListViewBinder::bindListItem);
        doReturn(chipView).when(view).findViewById(R.id.csn_chip);
        Resources resources = mock(Resources.class);
        doReturn(resources).when(view).getResources();
        doReturn(10).when(resources).getDimensionPixelSize(anyInt());

        GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final String longUrl =
                "longextendedsubdomainnamewithoutdashesinordertotestwordwrapping.badssl.com";
        doReturn(longUrl)
                .when(mUrlFormatterJniMock)
                .formatUrlForSecurityDisplay(eq(url), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS));
        model.set(ListItemProperties.URL, url);
        Assert.assertEquals(longUrl, textView.getText());
        Assert.assertEquals(TextUtils.TruncateAt.START, textView.getEllipsize());
        // Testing that ellipsization actually occurs in a Robolectric test isn't feasible as;
        // - {@link ShadowTextView} doesn't fully support layout so ellipsization won't occur.
        // - {@link ShadowTextUtils#ellipsize()} is implemented only as truncation  (for END)
        //   so it isn't possible to simulate easily.
        // At this point we should trust the Android Framework to do the right thing as the
        // text and configuration is correct.
    }

    @Test
    public void testBindRootView() {
        View view = mock(View.class);
        RecyclerView recyclerView = mock(RecyclerView.class);
        TextView textView = mock(TextView.class);
        ImageView imageView = mock(ImageView.class);
        InOrder inOrder = inOrder(view, textView, imageView, recyclerView);
        PropertyModel model = new PropertyModel(ContinuousSearchListProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                model, view, ContinuousSearchListViewBinder::bindRootView);

        doReturn(imageView).when(view).findViewById(eq(R.id.button_dismiss));
        doReturn(textView).when(view).findViewById(eq(R.id.continuous_search_provider_label));
        doReturn(recyclerView).when(view).findViewById(eq(R.id.recycler_view));

        int color = 0xAABBCC;
        model.set(ContinuousSearchListProperties.BACKGROUND_COLOR, color);
        inOrder.verify(view).setBackgroundColor(eq(color));

        model.set(ContinuousSearchListProperties.FOREGROUND_COLOR, color);
        inOrder.verify(imageView).setColorFilter(eq(color));

        View.OnClickListener listener = (v) -> {};
        model.set(ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK, listener);
        inOrder.verify(imageView).setOnClickListener(eq(listener));

        int position = 5;
        model.set(ContinuousSearchListProperties.SELECTED_ITEM_POSITION, position);
        inOrder.verify(recyclerView).smoothScrollToPosition(position);

        // Provider properties

        int iconRes = 11;
        model.set(ContinuousSearchListProperties.PROVIDER_ICON_RESOURCE, iconRes);
        inOrder.verify(textView).setCompoundDrawablesRelativeWithIntrinsicBounds(
                eq(iconRes), eq(0), eq(0), eq(0));

        final String label = "Label";
        model.set(ContinuousSearchListProperties.PROVIDER_LABEL, label);
        inOrder.verify(textView).setText(eq(label));

        int id = 123;
        model.set(ContinuousSearchListProperties.PROVIDER_TEXT_STYLE, id);
        inOrder.verify(textView).setTextAppearance(any(), eq(id));

        model.set(ContinuousSearchListProperties.PROVIDER_CLICK_LISTENER, listener);
        inOrder.verify(textView).setOnClickListener(eq(listener));
    }
}
