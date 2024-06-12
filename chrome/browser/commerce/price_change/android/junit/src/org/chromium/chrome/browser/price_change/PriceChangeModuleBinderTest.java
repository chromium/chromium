// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/** Test relating to binding for price change module. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceChangeModuleBinderTest {

    private static final String MODULE_TITLE = "module title";
    private static final String PRODUCT_TITLE = "product foo";
    private static final String CURRENT_PRICE = "$100";
    private static final String PREVIOUS_PRICE = "$150";
    private static final String PRODUCT_URL_DOMAIN = "foo.com";

    private Activity mActivity;
    private PriceChangeModuleView mView;
    private PropertyModel mModel;
    private Bitmap mBitmap;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mBitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        mView =
                (PriceChangeModuleView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.price_change_module_layout, null);
        mModel = new PropertyModel(PriceChangeModuleProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, PriceChangeModuleViewBinder::bind);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
    }

    @Test
    @SmallTest
    public void testSetModuleTitle() {
        TextView moduleTitleView = mView.findViewById(R.id.header_text);
        assertEquals("", moduleTitleView.getText());

        mModel.set(PriceChangeModuleProperties.MODULE_TITLE, MODULE_TITLE);

        assertEquals(MODULE_TITLE, moduleTitleView.getText());
    }

    @Test
    @SmallTest
    public void testSetProductTitle() {
        TextView productTitleView = mView.findViewById(R.id.product_title);
        assertEquals("", productTitleView.getText());

        mModel.set(PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING, PRODUCT_TITLE);

        assertEquals(PRODUCT_TITLE, productTitleView.getText());
    }

    @Test
    @SmallTest
    public void testSetFavicon() {
        ImageView faviconView = mView.findViewById(R.id.favicon_image);
        assertNull(faviconView.getDrawable());

        mModel.set(PriceChangeModuleProperties.MODULE_FAVICON_BITMAP, mBitmap);

        assertNotNull(faviconView.getDrawable());
    }

    @Test
    @SmallTest
    public void testSetCurrentPrice() {
        TextView currentPriceView = mView.findViewById(R.id.current_price);
        assertEquals("", currentPriceView.getText());

        mModel.set(PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING, CURRENT_PRICE);

        assertEquals(CURRENT_PRICE, currentPriceView.getText());
    }

    @Test
    @SmallTest
    public void testSetPreviousPrice() {
        TextView previousPriceView = mView.findViewById(R.id.previous_price);
        assertEquals("", previousPriceView.getText());

        mModel.set(PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING, PREVIOUS_PRICE);

        assertEquals(PREVIOUS_PRICE, previousPriceView.getText());
    }

    @Test
    @SmallTest
    public void testSetDomainString() {
        TextView domainView = mView.findViewById(R.id.price_drop_domain);
        assertEquals("", domainView.getText());

        mModel.set(PriceChangeModuleProperties.MODULE_DOMAIN_STRING, PRODUCT_URL_DOMAIN);

        assertEquals(PRODUCT_URL_DOMAIN, domainView.getText());
    }

    @Test
    @SmallTest
    public void testSetProductImage() {
        ImageView productImageView = mView.findViewById(R.id.product_image);
        assertNull(productImageView.getDrawable());

        mModel.set(PriceChangeModuleProperties.MODULE_PRODUCT_IMAGE_BITMAP, mBitmap);

        assertNotNull(productImageView.getDrawable());
    }

    @Test
    @SmallTest
    public void testSetOnClickListener() {
        AtomicBoolean buttonClicked = new AtomicBoolean();
        buttonClicked.set(false);
        mView.performClick();
        assertFalse(buttonClicked.get());

        mModel.set(
                PriceChangeModuleProperties.MODULE_ON_CLICK_LISTENER,
                (View view) -> buttonClicked.set(true));

        mView.performClick();
        assertTrue(buttonClicked.get());
    }

    @Test
    @SmallTest
    public void testSetModuleAccessibilityLabel() {
        String accessibilityLabel = "label";
        assertNull(mView.getContentDescription());

        mModel.set(PriceChangeModuleProperties.MODULE_ACCESSIBILITY_LABEL, accessibilityLabel);

        assertEquals(
                accessibilityLabel,
                mModel.get(PriceChangeModuleProperties.MODULE_ACCESSIBILITY_LABEL));
    }
}
