package org.chromium.chrome.browser.shopping.front_door;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

public class ProductLineParser {
    private static final String BRANDS_LIST_KEY = "brands";
    private static final String BRAND_NAME_KEY = "name";
    private static final String BRAND_MID_KEY = "brandMid";
    private static final String BRAND_IMAGE_URL_KEY = "imageUrl";
    private static final String BRAND_CATEGORY_LIST_KEY = "categories";
    private static final String PRODUCT_LINES_LIST_KEY = "productLines";
    private static final String PRODUCT_LINE_NAME_KEY = "name";
    private static final String PRODUCT_LINE_IMAGE_URL_KEY = "representativeImageUrl";
    private static final String PRODUCT_LINE_SRP_URL_KEY = "generatedSearchResultsUrl";

    public static void parse(String json, Callback<ProductLineInfo> callback) {
        try {
            JSONObject jsonObject = new JSONObject(json);
            JSONArray brands = jsonObject.getJSONArray(BRANDS_LIST_KEY);

            for (int i = 0; i < brands.length(); i++) {
                JSONObject brand = brands.getJSONObject(i);

                String brandName = brand.getString(BRAND_NAME_KEY);
                String brandUrl = "";
                if (brand.has(BRAND_IMAGE_URL_KEY)) {
                    brandUrl = brand.getString(BRAND_IMAGE_URL_KEY);
                }
                String brandMid = brand.getString(BRAND_MID_KEY);

                JSONArray categoryKeys = brand.getJSONArray(BRAND_CATEGORY_LIST_KEY);
                List<String> categoryKeyList = getCategoryKeys(categoryKeys);

                JSONArray productLines = brand.getJSONArray(PRODUCT_LINES_LIST_KEY);
                for (int j = 0; j < productLines.length(); j++) {
                    JSONObject productLine = productLines.getJSONObject(j);

                    String productLineName = productLine.getString(PRODUCT_LINE_NAME_KEY);
                    String productLineImageUrl = productLine.getString(PRODUCT_LINE_IMAGE_URL_KEY);
                    String productLineSrpUrl = productLine.getString(PRODUCT_LINE_SRP_URL_KEY);
                    callback.onResult(new ProductLineInfo.Builder()
                                              .withBrand(brandName)
                                              .withBrandMid(brandMid)
                                              .withBrandUrl(brandUrl)
                                              .withProductLineName(productLineName)
                                              .withImageUrl(productLineImageUrl)
                                              .withClickingUrl(productLineSrpUrl)
                                              .withCategoryKeyList(categoryKeyList)
                                              .build());
                }
            }
        } catch (JSONException e) {
            Log.e("Meil",
                    String.format(
                            "There was a problem parsing the JSON\n Details: %s", e.getMessage()));
        }
    }

    private static List<String> getCategoryKeys(JSONArray categoryKeys) {
        List<String> keys = new ArrayList<>();

        try {
            for (int i = 0; i < categoryKeys.length(); i++) {
                keys.add(categoryKeys.getString(i));
            }
        } catch (JSONException e) {
            Log.e("Meil",
                    String.format(
                            "There was a problem parsing the brand category keys\n Details: %s",
                            e.getMessage()));
        }

        return keys;
    }
}
