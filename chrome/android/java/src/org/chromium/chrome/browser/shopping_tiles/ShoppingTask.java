package org.chromium.chrome.browser.shopping_tiles;

import java.util.List;

public class ShoppingTask {
    public String name;
    private List<ProductInfo> mProductList;
    public ShoppingTask(String name, List<ProductInfo> productList) {
        this.name = name;
        this.mProductList = productList;
    }
    public List<ProductInfo> getProductList() {
        return mProductList;
    }
}
