package org.chromium.chrome.browser.shopping.front_door;

import java.util.List;
import java.util.Map;
import org.chromium.base.Callback;

public interface OnboardCategoryAndBrandProvider {
  void getBrandsForCategoriesWithCallback(List<String> categoryKeys, Callback<Map<String, List<BrandInfo>>> callback);
}