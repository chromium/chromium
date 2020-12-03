package org.chromium.chrome.browser.shopping.front_door;

import org.chromium.base.Callback;

import java.util.List;

public interface ProductLineProvider {
    // TODO(meiliang): Add size info after pagination is supported. Get x number of product lines.
    /**
     * Get product lines for the given list of brand ids. The list of brand ids can be empty. If
     * it's empty, the end point should return product lines from a default list.
     */
    void getProductLinesWithCallback(
            List<String> brandIds, Callback<List<ProductLineInfo>> callback);

    /**
     * @param identifier Identifier to use to filter the product info.
     * @return A list of {@link ProductLineInfo}s that are related to the given category identifier.
     */
    List<ProductLineInfo> getProductLinesForCategory(String identifier);

    /**
     * @param identifier Identifier to use to filter the product info.
     * @return A list of {@link ProductLineInfo}s that are related to the given brand identifier.
     */
    List<ProductLineInfo> getProductLinesForBrand(String identifier);

    /**
     * This is used after fetching.
     * @return A list of Client side {@link ProductLineInfo}.
     */
    List<ProductLineInfo> getAllProductLines();
}