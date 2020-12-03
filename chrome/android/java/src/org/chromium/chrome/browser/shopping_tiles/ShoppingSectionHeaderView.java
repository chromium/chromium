package org.chromium.chrome.browser.shopping_tiles;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;

public class ShoppingSectionHeaderView extends LinearLayout {
    // Views in the header layout that are set during inflate.
    private TextView mTitleView;
    private ListMenuButton mMenuView;

    private boolean mHasMenu;

    public ShoppingSectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(R.id.header_title);
        mMenuView = findViewById(R.id.header_menu);

        setHasMenu(false);
    }

    private void displayMenu() {
        assert mHasMenu;
    }

    public void setHeaderTitle(String title) {
        mTitleView.setText(title);
    }

    public void setHasMenu(boolean hasMenu) {
        mHasMenu = hasMenu;

        if (mHasMenu) {
            mMenuView.setVisibility(VISIBLE);
            mMenuView.setOnClickListener((View v) -> { displayMenu(); });
        } else {
            mMenuView.setVisibility(GONE);
        }
    }
}
