package org.chromium.chrome.browser.shopping_tiles;

import org.chromium.base.Callback;

import java.util.List;

public interface ShoppingProductsProvider {
    List<ProductInfo> getProductList();
    void getProductWithCallback(Callback<List<ProductInfo>> callback);
}
