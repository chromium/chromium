package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

// Contains a RoundedCornerImageView and a scrim at the bottom of the ImageView. Scrim can be turn
// off. Additional view can be added inside the scrim view.
public class ScrimableImageContainer extends FrameLayout {
    private ImageView mImageView;
    private AdjustableScrimView mScrimView;

    public ScrimableImageContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        LayoutInflater.from(getContext()).inflate(R.layout.scrimable_image, this);
        mImageView = findViewById(R.id.image);
        mScrimView = (AdjustableScrimView) findViewById(R.id.scrim);
        scrimViewVisibility(GONE);
    }

    public void scrimViewVisibility(int visibility) {
        mScrimView.setVisibility(visibility);
    }

    public void insertToScrimView(View view) {
        mScrimView.addView(view);
    }

    public void setImageBitmap(Bitmap bitmap) {
        mImageView.setImageBitmap(bitmap);
    }
}
