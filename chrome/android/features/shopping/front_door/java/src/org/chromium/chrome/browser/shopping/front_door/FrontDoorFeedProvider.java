package org.chromium.chrome.browser.shopping.front_door;

import org.chromium.base.Callback;
import org.chromium.base.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

// TODO(meiliang): Migrate the offer product here as well. Move the logic from DummyProductProvider
//  to here.
// Feed data storage for Front door.
public class FrontDoorFeedProvider
        implements ProductLineProvider, OnboardCategoryAndBrandProvider {
    private static final boolean DEBUG = true;
    private static final boolean USING_DUMMY_JSON = false;

    private List<ProductLineInfo> mAvailableProductLines = new ArrayList<>();
    private List<ProductLineInfo> mProvidedProductLines = new ArrayList<>();

    private Map<String, List<ProductLineInfo>> mBrandIdToProductLinesMap = new LinkedHashMap<>();
    private Map<String, Set<String>> mCategoryKeyToBrandIdsMap = new LinkedHashMap<>();

    private void writeResponseToFile(String response, String path) {
        try {
            File file = new File(path);
            file.createNewFile();
            FileOutputStream fout = new FileOutputStream(file);
            OutputStreamWriter outputStreamWriter = new OutputStreamWriter(fout);
            outputStreamWriter.write(response);
            outputStreamWriter.close();

            fout.flush();
            fout.close();
        } catch (IOException e) {
            Log.e("Meil", "File write failed: " + e.toString());
        }
    }

    // ProductLineProvider implementations.
    @Override
    public void getProductLinesWithCallback(
            List<String> brandIds, Callback<List<ProductLineInfo>> callback) {
        // TODO(meiliang): No need to fetch server after pagination is supported.

        resetProductLineData();
        if (USING_DUMMY_JSON) {
            ProductLineParser.parse(
                    DummyJSONResponseProvider.PRODUCT_LINE_JSON, this::addProductLineToQueue);
            supplyProductLinesToCallback(callback);
            return;
        }

        ShoppingFeedFetcher.fetchProductLine(brandIds, (response) -> {
            Log.e("Meil", "Product line Response: " + response);
            // TODO(meiliang): Use needFilters to skip filter support, and remove the
            // #resetProductLineData call.
            boolean needFilters = brandIds.size() != 0;
            if (DEBUG) writeResponseToFile(response, "/sdcard/productLineResponse.txt");
            ProductLineParser.parse(response, this::addProductLineToQueue);
            supplyProductLinesToCallback(callback);
        });
    }

    private void resetProductLineData() {
        mCategoryKeyToBrandIdsMap.clear();
        mBrandIdToProductLinesMap.clear();
        mAvailableProductLines.clear();
        mProvidedProductLines.clear();
    }

    private void addProductLineToQueue(ProductLineInfo productLineInfo) {
        for (String categoryKey : productLineInfo.categoryKeyList) {
            if (!mCategoryKeyToBrandIdsMap.containsKey(categoryKey)) {
                mCategoryKeyToBrandIdsMap.put(categoryKey, new HashSet<>());
            }

            mCategoryKeyToBrandIdsMap.get(categoryKey).add(productLineInfo.brandMid);
        }

        if (!mBrandIdToProductLinesMap.containsKey(productLineInfo.brandMid)) {
            mBrandIdToProductLinesMap.put(productLineInfo.brandMid, new ArrayList<>());
        }

        mBrandIdToProductLinesMap.get(productLineInfo.brandMid).add(productLineInfo);

        mAvailableProductLines.add(productLineInfo);
    }

    private void supplyProductLinesToCallback(Callback<List<ProductLineInfo>> callback) {
        mProvidedProductLines.addAll(mAvailableProductLines);
        mAvailableProductLines.clear();
        callback.onResult(mProvidedProductLines);
    }

    @Override
    public List<ProductLineInfo> getProductLinesForCategory(String identifier) {
        Log.e("Meil", "getProductLinesForCategory: " + identifier);
        assert mCategoryKeyToBrandIdsMap.containsKey(identifier);
        List<ProductLineInfo> infoList = new ArrayList<>();

        for (String brandId : mCategoryKeyToBrandIdsMap.get(identifier)) {
            infoList.addAll(getProductLinesForBrand(brandId));
        }

        return infoList;
    }

    @Override
    public List<ProductLineInfo> getProductLinesForBrand(String identifier) {
        assert mBrandIdToProductLinesMap.containsKey(identifier);
        return mBrandIdToProductLinesMap.get(identifier);
    }

    @Override
    public List<ProductLineInfo> getAllProductLines() {
        List<ProductLineInfo> infoList = new ArrayList<>();

        for (List<ProductLineInfo> brandProductLineInfo : mBrandIdToProductLinesMap.values()) {
            infoList.addAll(brandProductLineInfo);
        }

        return infoList;
    }

    // OnboardCategoryAndBrandProvider implementations
    @Override
    public void getBrandsForCategoriesWithCallback(
            List<String> categoryKeys, Callback<Map<String, List<BrandInfo>>> callback) {
        Log.e("Meil_FrontDoorFeedProvider", "getBrandsForCategories: " + categoryKeys.toString());

        Map<String, List<BrandInfo>> resultMap = new LinkedHashMap<>();

        for (String key : categoryKeys) {
            resultMap.put(key, new ArrayList<>());
        }

        if (USING_DUMMY_JSON) {
            OnboardBrandParser.parse(
                    DummyJSONResponseProvider.ONBOARDING_BRAND_LIST_JSON, resultMap);
            callback.onResult(resultMap);
            return;
        }

        ShoppingFeedFetcher.fetchBrandsForCategories(categoryKeys, (response) -> {
            Log.e("Meil_FrontDoorFeedProvider", "onboard brand Response: " + response);
            if (DEBUG) writeResponseToFile(response, "/sdcard/onboardBrandResponse.txt");
            OnboardBrandParser.parse(response, resultMap);
            callback.onResult(resultMap);
        });
    }
}
