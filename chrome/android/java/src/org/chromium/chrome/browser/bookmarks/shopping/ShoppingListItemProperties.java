
package org.chromium.chrome.browser.bookmarks.shopping;

import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChipView;

class ShoppingListItemProperties {
    static final WritableLongPropertyKey ID = new WritableLongPropertyKey();
    static final WritableObjectPropertyKey<String> ID_STRING = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> DOMAIN =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PRICE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ListMenuButtonDelegate> MENU_DELEGATE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> CLICK_DELEGATE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Bitmap> IMAGE =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey USING_FAVICON =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ID, ID_STRING, TITLE, DOMAIN,
            PRICE, MENU_DELEGATE, CLICK_DELEGATE, IMAGE, USING_FAVICON};

    private static int sTargetWidth;

    public static void bindListItem(PropertyModel model, View view, PropertyKey propertyKey) {
        ViewGroup group = (ViewGroup) view;
        if (TITLE == propertyKey) {
            ((TextView) group.findViewById(R.id.title)).setText(model.get(TITLE));
        } else if (DOMAIN == propertyKey) {
            ((TextView) group.findViewById(R.id.caption)).setText(model.get(DOMAIN));
        } else if (PRICE == propertyKey) {
            ChipView chip = ((ChipView) group.findViewById(R.id.price_chip));
            if (model.get(PRICE) == null) {
                chip.getPrimaryTextView().setText("");
                chip.setVisibility(View.GONE);
            } else {
                chip.getPrimaryTextView().setText(model.get(PRICE));
                chip.setVisibility(View.VISIBLE);
            }
        } else if (MENU_DELEGATE == propertyKey) {
            ((ListMenuButton) group.findViewById(R.id.more)).setDelegate(model.get(MENU_DELEGATE));
        } else if (CLICK_DELEGATE == propertyKey) {
            Runnable delegate = model.get(CLICK_DELEGATE);
            group.setClickable(delegate != null);
            if (delegate != null) {
                group.setOnClickListener((v) -> delegate.run());
            } else {
                group.setOnClickListener(null);
            }
        } else if (IMAGE == propertyKey) {
            //RoundedCornerImageView thumbnail =
            //        (RoundedCornerImageView) group.findViewById(R.id.thumbnail);
            Bitmap bitmap = model.get(IMAGE);

            if (bitmap != null && !model.get(USING_FAVICON)) {
                int targetWidth = view.getWidth();
                if (targetWidth <= 0 && view.getParent() != null) {
                    targetWidth = ((View) view.getParent()).getWidth();
                }
                if (sTargetWidth <= 0 || targetWidth > 0) sTargetWidth = targetWidth;
                float maxHeight =
                        ((float) sTargetWidth / bitmap.getWidth()) * bitmap.getHeight();

                RoundedCornerImageView newImageView = new RoundedCornerImageView(view.getContext());
                newImageView.setLayoutParams(
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, (int) maxHeight));
                newImageView.setImageBitmap(bitmap);
                newImageView.setMinimumHeight((int) maxHeight);
                newImageView.setScaleType(ImageView.ScaleType.CENTER_CROP);

                int radius = view.getResources().getDimensionPixelSize(
                        R.dimen.shopping_thumbnail_corner_radius);
                newImageView.setRoundedCorners(radius, radius, radius, radius);

                ViewGroup container = view.findViewById(R.id.thumbnail_container);
                if (container.getChildAt(0) instanceof RoundedCornerImageView) {
                    container.removeViewAt(0);
                }
                container.addView(newImageView, 0);


            } else {
                RoundedCornerImageView newImageView = new RoundedCornerImageView(view.getContext());
                newImageView.setLayoutParams(
                        new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 200));

                if (bitmap != null && model.get(USING_FAVICON)) {
                    newImageView.setImageBitmap(bitmap);
                } else {
                    newImageView.setImageResource(R.drawable.ic_shopping_bag_24dp);
                }

                newImageView.setMinimumHeight(300);
                newImageView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);

                int radius = view.getResources().getDimensionPixelSize(
                        R.dimen.shopping_thumbnail_corner_radius);
                newImageView.setRoundedCorners(radius, radius, radius, radius);

                ViewGroup container = view.findViewById(R.id.thumbnail_container);
                if (container.getChildAt(0) instanceof RoundedCornerImageView) {
                    container.removeViewAt(0);
                }
                container.addView(newImageView, 0);
            }
        }
    }

    private static void setBitmapOnView(
            RoundedCornerImageView thumbnail, Bitmap bitmap, int targetWidth) {
        float maxHeight =
                ((float) targetWidth / bitmap.getWidth()) * bitmap.getHeight();
        Bitmap scaledBitmap = Bitmap.createScaledBitmap(
                bitmap, targetWidth, (int) maxHeight, false);
        thumbnail.getLayoutParams().height = scaledBitmap.getHeight();
        thumbnail.setImageBitmap(scaledBitmap);
        thumbnail.setScaleType(ImageView.ScaleType.CENTER_CROP);
    }
}


