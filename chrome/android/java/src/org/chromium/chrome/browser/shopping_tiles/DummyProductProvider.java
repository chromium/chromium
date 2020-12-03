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
    private void parse(String json, Callback<ProductInfo> addProductCallback) {
        Log.e("Meil", "parse response");
        try {
            JSONObject jsonObject = new JSONObject(json);
            JSONArray jsonModules = jsonObject.getJSONArray("modules");
            Log.e("Meil", "modules size: " + jsonModules.length());

            for (int i = 0; i < jsonModules.length() && (mSize == -1 || mProducts.size() < mSize);
                    i++) {
                // Module
                JSONObject module = jsonModules.getJSONObject(i);
                if (!module.has("collections")) {
                    Log.e("Meil", "Module does not has collections");
                    continue;
                }
                JSONArray moduleCollection = module.getJSONArray("collections");

                // ModuleCollection
                Log.e("Meil", "Module Collection size: " + moduleCollection.length());
                for (int j = 0;
                        j < moduleCollection.length() && (mSize == -1 || mProducts.size() < mSize);
                        j++) {
                    JSONObject collection = moduleCollection.getJSONObject(j);
                    String collectionTitle = collection.getString("title");
                    String collectionCategory = collection.getString("category");
                    Log.e("Meil",
                            "Collection: " + collectionTitle + "; category: " + collectionCategory);
                    JSONArray collectionProducts = collection.getJSONArray("products");

                    // Product list in collection
                    Log.e("Meil", "products size: " + collectionProducts.length());
                    for (int k = 0; k < collectionProducts.length()
                            && (mSize == -1 || mProducts.size() < mSize);
                            k++) {
                        Log.e("Meil", "Product:");
                        JSONObject jsonProduct = collectionProducts.getJSONObject(k);

                        String name = jsonProduct.getString("title");
                        String url = "";
                        if (jsonProduct.has("productClickUrl")) {
                            url = jsonProduct.getString("productClickUrl");
                            JSONObject urlObject = jsonProduct.getJSONObject("productClickUrl");
                            url = urlObject.getString("url");
                        }

                        String imageUrl = jsonProduct.getString("imageUrl");

                        if (!jsonProduct.has("currentPrice")) {
                            Log.e("Meil", "Product does not has price");
                            continue;
                        }

                        JSONObject priceObject = jsonProduct.getJSONObject("currentPrice");
                        String price = priceObject.getString("amountMicros");

                        Log.e("Meil", "product title: " + name);

                        addProductCallback.onResult(new ProductInfo.Builder()
                                                            .withName(name)
                                                            .withUrl(url)
                                                            .withImageUrl(imageUrl)
                                                            .withPriceStr(price)
                                                            .build());
                    }
                }
            }
            mCallback.onResult(mProducts);
        } catch (JSONException e) {
            Log.e("Meil",
                    String.format(
                            "There was a problem parsing the JSON\n Details: %s", e.getMessage()));
        }
    }

    private List<ProductInfo> mProducts = new ArrayList<>();
    private Callback<List<ProductInfo>> mCallback;
    private int mSize;
    private static final boolean DEBUG = true;
    private static final boolean USING_DUMMY_JSON = false;

    public DummyProductProvider(int size) {
        mSize = size;
    }

    private void addProduct(ProductInfo productInfo) {
        mProducts.add(productInfo);
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
