package org.chromium.chrome.browser.shopping_tiles;

import android.graphics.Bitmap;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.ProductLineProperties;
import org.chromium.components.query_tiles.QueryTileConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class ProductLineItemViewBinder {
    public static void bind(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        Log.d("Meil", "Bind Product Line Item");
        if (propertyKey == ProductLineProperties.PRODUCT_NAME) {
            TextView productName = view.findViewById(R.id.product_name);
            productName.setText(model.get(ProductLineProperties.PRODUCT_NAME));
        } else if (propertyKey == ProductLineProperties.BRAND_NAME) {
            TextView brandName = view.findViewById(R.id.brand_name);
            brandName.setText(model.get(ProductLineProperties.BRAND_NAME));
        } else if (propertyKey == ProductLineProperties.IMAGE_FETCHER) {
            ImageView imageView = (ImageView) view.findViewById(R.id.image_container);
            Callback<Bitmap> callback = result -> {
                if (result == null) {
                    Log.e("Meil", "Fetcher return null bitmap");
                } else {
                    imageView.setImageBitmap(result);
                }
            };

            String imageUrl = model.get(ProductLineProperties.IMAGE_URL);
            ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(imageUrl,
                    ImageFetcher.SHOPPING_TILE_UMA_CLIENT_NAME, imageView.getWidth(),
                    imageView.getHeight(), QueryTileConstants.IMAGE_EXPIRATION_INTERVAL_MINUTES);

            model.get(ProductLineProperties.IMAGE_FETCHER).fetchImage(params, callback);
        } else if (propertyKey == ProductLineProperties.ON_CLICK_CALLBACK) {
            String url = model.get(ProductLineProperties.SRP_URL);
            view.setOnClickListener(
                    (v) -> { model.get(ProductLineProperties.ON_CLICK_CALLBACK).onResult(url); });
        } else if (propertyKey == ProductLineProperties.ITEM_CONTEXT_MENU_DELEGATE) {
            model.get(ProductLineProperties.ITEM_CONTEXT_MENU_DELEGATE)
                    .setItemPropertyModel(model, false);
            view.setOnCreateContextMenuListener(
                    model.get(ProductLineProperties.ITEM_CONTEXT_MENU_DELEGATE));
        }
    }
}
