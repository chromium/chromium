package org.chromium.chrome.browser.shopping.front_door;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.shopping.front_door.ShoppingFeedFetcher.CountryCodeProvider;

import java.util.List;
import java.util.Map;

public interface OnboardCategoryAndBrandProvider {
    void getBrandsForCategoriesWithCallback(List<String> categoryKeys,
            Callback<Map<String, List<BrandInfo>>> callback,
            CountryCodeProvider countryCodeProvider);
}