package org.chromium.chrome.browser.shopping_tiles;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.shopping.front_door.DummyJSONResponseProvider;
import org.chromium.chrome.browser.shopping.front_door.ShoppingFeedFetcher;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.List;

public class DummyProductProvider implements ShoppingProductsProvider {
    private static final String RECENTLY_VIEW_KEY = "Recently Viewed";

    private static boolean validKey(JSONObject jsonObject, String key) {
        if (jsonObject.has(key)) return true;
        Log.e("Meil_productParser", "Product object missing: " + key);
        return false;
    }

    private void parse(String json, Callback<ProductInfo> addProductCallback) {
        Log.e("Meil_productParser", "parse response");
        try {
            JSONObject jsonObject = new JSONObject(json);

            if (!validKey(jsonObject, "modules")) {
                return;
            }
            JSONArray jsonModules = jsonObject.getJSONArray("modules");
            Log.e("Meil_productParser", "modules size: " + jsonModules.length());

            for (int i = 0; i < jsonModules.length() && (mSize == -1 || mProducts.size() < mSize);
                    i++) {
                // Module
                JSONObject module = jsonModules.getJSONObject(i);
                if (!module.has("collections")) {
                    Log.e("Meil_productParser", "Module does not has collections");
                    continue;
                }
                JSONArray moduleCollection = module.getJSONArray("collections");

                // ModuleCollection
                Log.e("Meil_productParser", "Module Collection size: " + moduleCollection.length());
                for (int j = 0;
                        j < moduleCollection.length() && (mSize == -1 || mProducts.size() < mSize);
                        j++) {
                    JSONObject collection = moduleCollection.getJSONObject(j);
                    if (!validKey(collection, "title")) {
                        continue;
                    }
                    String collectionTitle = collection.getString("title");

                    boolean isRecentlyViewed = false;
                    if (collectionTitle.compareTo(RECENTLY_VIEW_KEY) == 0) {
                        isRecentlyViewed = true;
                    }

                    if (!validKey(collection, "category")) {
                        continue;
                    }
                    String collectionCategory = collection.getString("category");
                    Log.e("Meil_productParser",
                            "Collection: " + collectionTitle + "; category: " + collectionCategory);

                    if (!validKey(collection, "products")) {
                        continue;
                    }
                    JSONArray collectionProducts = collection.getJSONArray("products");

                    // Product list in collection
                    Log.e("Meil_productParser", "products size: " + collectionProducts.length());
                    for (int k = 0; k < collectionProducts.length()
                            && (mSize == -1 || mProducts.size() < mSize);
                            k++) {
                        Log.e("Meil_productParser", "Product:");
                        JSONObject jsonProduct = collectionProducts.getJSONObject(k);

                        if (!validKey(jsonProduct, "title")) {
                            continue;
                        }
                        String name = jsonProduct.getString("title");
                        String url = "";
                        if (jsonProduct.has("productClickUrl")) {
                            JSONObject urlObject = jsonProduct.getJSONObject("productClickUrl");
                            if (!validKey(urlObject, "url")) {
                                continue;
                            }
                            url = urlObject.getString("url");
                        } else {
                            Log.e("Meil_productParser", "Product object missing: productClickUrl");
                            continue;
                        }

                        if (!validKey(jsonProduct, "imageUrl")) {
                            continue;
                        }
                        String imageUrl = jsonProduct.getString("imageUrl");

                        if (!jsonProduct.has("currentPrice")) {
                            Log.e("Meil_productParser", "Product does not has price");
                            continue;
                        }

                        JSONObject priceObject = jsonProduct.getJSONObject("currentPrice");
                        if (!validKey(priceObject, "amountMicros")) {
                            continue;
                        }
                        String priceStr = priceObject.getString("amountMicros");
                        float price = Float.parseFloat(priceStr);

                        Log.e("Meil_productParser", "product title: " + name);

                        addProductCallback.onResult(new ProductInfo.Builder()
                                                            .withName(name)
                                                            .withUrl(url)
                                                            .withImageUrl(imageUrl)
                                                            .withPrice(price)
                                                            .withRecentlyView(isRecentlyViewed)
                                                            .build());
                    }
                }
            }
            mCallback.onResult(mProducts);
        } catch (JSONException e) {
            Log.e("Meil",
                    String.format(
                            "There was a problem parsing the offer product JSON\n Details: %s",
                            e.getMessage()));
        }
    }

    private List<ProductInfo> mProducts = new ArrayList<>();

    private List<ProductInfo> mRecentlyViewProducts = new ArrayList<>();
    private List<ProductInfo> mRecommendedProducts = new ArrayList<>();
    private Callback<List<ProductInfo>> mCallback;
    private int mSize;
    private static final boolean DEBUG = true;
    private static final boolean USING_DUMMY_JSON = false;

    public DummyProductProvider(int size) {
        mSize = size;
    }

    private void addProduct(ProductInfo productInfo) {
        if (productInfo.isRecentlyView) {
            mRecentlyViewProducts.add(productInfo);
        } else {
            mRecommendedProducts.add(productInfo);
        }
        // mProducts.add(productInfo);
    }

    @Override
    public List<ProductInfo> getProductList() {
        return mProducts;
    }

    @Override
    public void getProductWithCallback(Callback<List<ProductInfo>> callback) {
        mCallback = callback;
        if (USING_DUMMY_JSON) {
            parse(DummyJSONResponseProvider.JUMP_BACK_IN_JSON);
        } else {
            ShoppingFeedFetcher.fetch(this::parse);
        }
    }

    public void parse(String response) {
        Log.e("Meil", "response: " + response);
        if (DEBUG) writeResponseToFile(response);
        parse(response, this::addProduct);
        supplyProductToCallback();
    }

    private void supplyProductToCallback() {
        for (int i = 0; i < mRecentlyViewProducts.size() && i < 4; i++) {
            mProducts.add(mRecentlyViewProducts.get(i));
        }
        int recentlyViewSize = mProducts.size();
        for (int i = 0; i < mRecommendedProducts.size() && i < 8 - recentlyViewSize; i++) {
            mProducts.add(mRecommendedProducts.get(i));
        }
        mCallback.onResult(mProducts);
    }

    private void writeResponseToFile(String response) {
        try {
            File file = new File("/sdcard/response.txt");
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
}
