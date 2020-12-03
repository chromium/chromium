package org.chromium.chrome.browser.shopping.front_door;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

public interface ChipsProvider {
    ListModel<PropertyModel> getChips();
}
