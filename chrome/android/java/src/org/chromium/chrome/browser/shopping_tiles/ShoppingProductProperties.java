package org.chromium.chrome.browser.shopping_tiles;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListMediator.ContextMenuDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

public class ShoppingProductProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> PRODUCT_NAME =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableFloatPropertyKey PRICE =
            new PropertyModel.WritableFloatPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> URL =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> PRICE_STR =
            new PropertyModel.WritableObjectPropertyKey<>();
    // TODO(meiliang): Change to a fetcher form, and fetch with a callback. when callback run it
    // updates the image
    public static final PropertyModel.WritableObjectPropertyKey<View> PRODUCT_IMAGE =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> IMAGE_URL =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<ImageFetcher> IMAGE_FETCHER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<Callback<String>> ON_CLICK_CALLBACK =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<Callback<String>> BOOKMARK_CLICK_CALLBACK =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_BOOKMARKED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<ContextMenuDelegate> ITEM_CONTEXT_MENU_DELEGATE =
            new WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_RECENTLY_VIEWED =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {PRODUCT_NAME, PRICE, URL,
            PRODUCT_IMAGE, PRICE_STR, IMAGE_FETCHER, IMAGE_URL, ON_CLICK_CALLBACK,
            BOOKMARK_CLICK_CALLBACK, IS_BOOKMARKED, ITEM_CONTEXT_MENU_DELEGATE, IS_RECENTLY_VIEWED};
}
