package org.chromium.chrome.browser.shopping_tiles;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

public class NTPTabLayout extends LinearLayout {
    public interface TabSelectionDelegate extends OnTabSelectedListener {
        int getSelectedTabIndex();
    }

    public interface MenuDelegate {
        interface Observer {
            void onSwitch(boolean isEnabled);
        }
        void addObserver(Observer observer);
        void removeObserver(Observer observer);
        ModelList getMenuModelList();
        ListMenu.Delegate getListMenuDelegate();
        boolean isEnabled();
    }

    private TabLayout mTabLayout;
    private TextView mOffContentView;
    private ListMenuButton mMenuButton;
    private boolean mIsOff;
    private TabSelectionDelegate mTabSelectionDelegate;

    private MenuDelegate mMenuDelegate;
    private MenuDelegate.Observer mMenuDelegateObserver;

    public NTPTabLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        Log.e("Meil_NTPTabLayout", "inflate");
        super.onFinishInflate();
        LayoutInflater.from(getContext()).inflate(R.layout.ntp_tablayout_view, this);

        mTabLayout = (TabLayout) findViewById(R.id.tab_layout);
        mMenuDelegateObserver = this::setState;

        mOffContentView = (TextView) findViewById(R.id.off_content);
        mMenuButton = (ListMenuButton) findViewById(R.id.menu);

        mMenuButton.setOnClickListener((v) -> { displayMenu(); });
    }

    private void displayMenu() {
        assert mTabSelectionDelegate != null : "Delegate must be set once";

        ModelList menuListItem = mMenuDelegate.getMenuModelList();
        if (menuListItem == null) {
            assert false : "No list items model to display the menu";
            return;
        }

        ListMenu.Delegate menuDelegate = mMenuDelegate.getListMenuDelegate();
        if (menuDelegate == null) {
            assert false : "No list menu delegate for the menu";
            return;
        }

        BasicListMenu listMenu = new BasicListMenu(getContext(), menuListItem, menuDelegate);

        ListMenuButtonDelegate delegate = new ListMenuButtonDelegate() {
            @Override
            public ListMenu getListMenu() {
                return listMenu;
            }

            @Override
            public RectProvider getRectProvider(View listMenuButton) {
                ViewRectProvider rectProvider = new ViewRectProvider(listMenuButton);
                rectProvider.setIncludePadding(true);
                rectProvider.setInsetPx(0, 0, 0, 0);
                return rectProvider;
            }
        };

        mMenuButton.setDelegate(delegate);
        mMenuButton.tryToFitLargestItem(true);
        mMenuButton.showMenu();
    }

    public void setDelegate(TabSelectionDelegate delegate) {
        mTabSelectionDelegate = delegate;

        mTabLayout.clearOnTabSelectedListeners();

        // Initial state to match the delegate.
        Tab delegateSelectedTab = mTabLayout.getTabAt(delegate.getSelectedTabIndex());
        delegateSelectedTab.select();

        mTabLayout.addOnTabSelectedListener(delegate);
    }

    public void setMenuDelegate(MenuDelegate delegate) {
        assert delegate != null : "delegate should not be null";

        if (mMenuDelegate != null) {
            mMenuDelegate.removeObserver(mMenuDelegateObserver);
        }

        mMenuDelegate = delegate;
        mMenuDelegate.addObserver(mMenuDelegateObserver);

        setState(mMenuDelegate.isEnabled());
    }

    private void setState(boolean isEnabled) {
        if (isEnabled) {
            mTabLayout.getTabAt(0).select();
        }
        mTabLayout.setVisibility(isEnabled ? VISIBLE : GONE);
        mOffContentView.setVisibility(isEnabled ? GONE : VISIBLE);
        // setBackgroundResource(mIsOff ? R.drawable.hairline_border_card_background : 0);
    }

    @Override
    protected void onVisibilityChanged(@NonNull View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);

        if (visibility == VISIBLE) {
            // Initial state to match the delegate.
            Tab delegateSelectedTab =
                    mTabLayout.getTabAt(mTabSelectionDelegate.getSelectedTabIndex());
            delegateSelectedTab.select();

            mTabLayout.addOnTabSelectedListener(mTabSelectionDelegate);
        } else {
            mTabLayout.clearOnTabSelectedListeners();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mTabLayout.clearOnTabSelectedListeners();
    }
}