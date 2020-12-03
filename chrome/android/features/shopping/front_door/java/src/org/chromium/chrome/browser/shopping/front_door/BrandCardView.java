package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

public class BrandCardView extends FrameLayout {
    private View mBrandInfo;
    private View mImagesView;

    public BrandCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LayoutInflater.from(getContext()).inflate(R.layout.brand_card_item, this);
        mBrandInfo = findViewById(R.id.brand_info);
        // mImagesView = findViewById(R.i)
    }
}
