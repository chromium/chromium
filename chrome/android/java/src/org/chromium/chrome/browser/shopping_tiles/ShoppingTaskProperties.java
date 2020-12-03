package org.chromium.chrome.browser.shopping_tiles;

import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class ShoppingTaskProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> TASK_NAME =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<Supplier<View>> PRODUCT_LIST_VIEW_SUPPLIER =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    public static final PropertyModel
            .WritableObjectPropertyKey<ShoppingTasksSection.ViewMoreHandler> VIEW_MORE_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableObjectPropertyKey<Boolean> REFRESH =
            new PropertyModel.WritableObjectPropertyKey(true);
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TASK_NAME, PRODUCT_LIST_VIEW_SUPPLIER, VIEW_MORE_HANDLER, REFRESH};
}
