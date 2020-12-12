package org.chromium.chrome.browser.shopping.front_door;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class OnboardBrandParser {
    private static final String CATEGORY_BRANDS_MAP_KEY = "categoryToBrands";
    private static final String BRANDS_LIST_KEY = "brands";

    private static final String BRAND_NAME_KEY = "name";
    private static final String BRAND_URL_KEY = "brandUrl";
    private static final String BRAND_MID_KEY = "brandMid";
    private static final String BRAND_CATEGORY_LIST_KEY = "categories";
    private static final String BRAND_REPRESENTATIVE_IMAGES_KEY = "representativeImageUrls";
    private static final String BRAND_LOGO_KEY = "imageUrl";

    public static void parse(
            String json, Map<String, List<BrandInfo>> categoryKeyToBrandResultMap) {
        try {
            JSONObject jsonObject = new JSONObject(json);
            JSONObject categoryToBrandsObject = jsonObject.getJSONObject(CATEGORY_BRANDS_MAP_KEY);

            Iterator<String> keysItr = categoryToBrandsObject.keys();

            while (keysItr.hasNext()) {
                String categoryKey = keysItr.next();
                Log.e("Meil_category_to_brand_parser", "Key: " + categoryKey);
                JSONObject brandsObject = (JSONObject) categoryToBrandsObject.get(categoryKey);

                JSONArray brands = brandsObject.getJSONArray(BRANDS_LIST_KEY);

                for (int i = 0; i < brands.length(); i++) {
                    JSONObject brand = brands.getJSONObject(i);

                    String brandName;
                    String brandUrl;
                    String brandMid;

                    if (brand.has(BRAND_NAME_KEY)) {
                        brandName = brand.getString(BRAND_NAME_KEY);
                    } else {
                        Log.e("Meil_brand_parser", "brand does not contain: " + BRAND_NAME_KEY);
                        continue;
                    }

                    if (brand.has(BRAND_URL_KEY)) {
                        brandUrl = brand.getString(BRAND_URL_KEY);
                    } else {
                        Log.e("Meil_brand_parser", "brand does not contain: " + BRAND_URL_KEY);
                        continue;
                    }

                    if (brand.has(BRAND_MID_KEY)) {
                        brandMid = brand.getString(BRAND_MID_KEY);
                    } else {
                        Log.e("Meil_brand_parser", "brand does not contain: " + BRAND_MID_KEY);
                        continue;
                    }

                    String brandLogoUrl = "";
                    if (brand.has(BRAND_LOGO_KEY)) {
                        brandLogoUrl = brand.getString(BRAND_LOGO_KEY);
                    } else {
                        Log.e("Meil_brand_parser", "brand does not contain: " + BRAND_LOGO_KEY);
                        continue;
                    }

                    List<String> representativeImageUrls = null;
                    if (brand.has(BRAND_REPRESENTATIVE_IMAGES_KEY)) {
                        JSONArray representativeImageUrlJson =
                                brand.getJSONArray(BRAND_REPRESENTATIVE_IMAGES_KEY);
                        representativeImageUrls =
                                convertJSONArrayToListString(representativeImageUrlJson);
                    }

                    if (representativeImageUrls == null || representativeImageUrls.size() != 3) {
                        Log.e("Meil_brand_parser", "representativeImageUrls not equal to 3");
                        continue;
                    }

                    BrandInfo brandInfo = new BrandInfo.Builder()
                                                  .withName(brandName)
                                                  .withId(brandMid)
                                                  .withUrl(brandUrl)
                                                  .withImageUrls(representativeImageUrls)
                                                  .withLogoUrl(brandLogoUrl)
                                                  .build();

                    categoryKeyToBrandResultMap.get(categoryKey).add(brandInfo);
                }
            }
            Log.e("Meil_category_to_brand_parser", "Result map: " + categoryKeyToBrandResultMap);
        } catch (JSONException e) {
            Log.e("Meil",
                    String.format(
                            "There was a problem parsing the Onboard brand JSON\n Details: %s",
                            e.getMessage()));
        }
    }

    private static List<String> convertJSONArrayToListString(JSONArray categoryKeys) {
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
