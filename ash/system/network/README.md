# System Tray - Network and VPN Pages

The network page within the system tray provides the user visibility into the
available networks and provides a convenient entrypoint for connecting to a new
network, or viewing the details of an existing network. The VPN page within the
system tray provides similar functionality, except this page is concerned with
VPNs instead of networks.

The network and VPN concepts are very similar in code and both of their pages
within the system tray share a similar structure. The code within this directory
was written with these similarities in mind, and much of it is shared between
the two distinct pages.

At a high-level, there are three different categories of classes:

* Views with minimal logic that create and own other views; these classes
  typically are used to present information
* Controller classes that are responsible for the creation and management of
  views, including notifying them when the data they present has changed
* "List" controller classes that are responsible for updating views to have the
  correct list of networks and VPNs. These classes are instantiated and owned by
  the aforementioned controller classes and improve testability.

## Views

The class hierarchy of the views used for the network and VPN pages has many
different layers. These different layers are a result of:

* A focus on testability
* A desire to share logic between pages

While complicated, the end result is that testing each class in isolation is
trivial and it becomes possible to introduce comprehensive test coverage for
this directory.

#### Hierarchy:
```
NetworkDetailedView
 └─NetworkDetailedNetworkView
   ├─FakeNetworkDetailedNetworkView
   └─NetworkDetailedNetworkViewImpl

NetworkListItemView
  └─NetworkListNetworkItemView

NetworkListHeaderView
  └─NetworkListNetworkHeaderView
    ├─FakeNetworkListNetworkHeaderView
    ├─NetworkListMobileHeaderView
    ├─NetworkListTetherHostsHeaderView
    └─NetworkListWifiHeaderView
```

### NetworkDetailedView

The [`NetworkDetailedView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_detailed_view.h;l=35;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
class is the top-level view used for the network page and intended to be used by
the VPN page. This class implements much of the core, shared logic such as
creating the "info" or "settings" buttons shown in the top-right of the pages.

The `NetworkDetailedView` class defines a
[`Delegate`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_detailed_view.h;l=40-50;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
class, and accepts an instance of this `Delegate` class in its constructor. This
pattern allows the view to notify its delegate, in this case implemented by a
controller, to be capable of notifying when a network or VPN is selected.

### NetworkDetailedNetworkView

The
[`NetworkDetailedNetworkView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_detailed_network_view.h;l=31;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
class defines the interface used to interact with the network page. This class
inherits from `NetworkDetailedView` but includes additional logic that is only
applicable to networks, such as APIs to create a WiFi network row or show the
WiFi scanning bar. This class is abstract to improve testability and is
implemented by
[`NetworkDetailedNetworkViewImpl`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_detailed_network_view_impl.h;l=24;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b).
The `NetworkDetailedNetworkView` class defines a
[`Delegate`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_detailed_network_view.h;l=35-60;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
class that extends `NetworkDetailedView::Delegate` to include network-specific
logic, such as when WiFi is enabled or disabled.

The primary responsibility of this class is to provide the APIs to create the
different views needed for the network page, and to own the different views
needed for the network page. Most of the logic required to update the network
page is delegated to different controllers.

### NetworkListItemView

The
[`NetworkListItemView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_item_view.h;l=19;drc=4c290b90230aa54fd676924d74aa311aa68c566b)
is an abstract class intended to define the interface used for the individual
network and VPN items within the lists of the network and VPN pages. This view
can only be updated by using its
[`UpdateViewForNetwork`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_item_view.h;l=27;drc=4c290b90230aa54fd676924d74aa311aa68c566b)
API and providing it updated network or VPN information.

### NetworkListNetworkItemView

The
[`NetworkListNetworkItemView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_network_item_view.h;l=23;drc=1854c614e8549f2b1dd0a891bf911d42323035cc)
extends `NetworkListItemView` to implement all of the logic needed by individual
network items with in the list on the network page.

### NetworkListHeaderView

The
[`NetworkListHeaderView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_header_view.h;l=21;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
defines the interface of the "header" views used in the network and VPN pages.
These header views are used to denote the beginning of a section within these
pages, such as the WiFi section or the list of VPNs for a specific provider.

### NetworkListNetworkHeaderView

The
[`NetworkListNetworkHeaderView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_network_header_view.h;l=25;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
class extends `NetworkListHeaderView` to implement additional logic and provide
additional APIs that are specific to headers within the network page. For
example, the network section has a toggle in each header that can be used to
enable and disable the corresponding technology. The VPN page does not have
these toggles.

This class defines additional APIs that are used to control the state of the
toggles, including whether it is on, enabled, and visible.

### NetworkList\*HeaderView

The
[`NetworkListMobileHeaderView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_mobile_header_view.h;l=15;drc=869ec54af88f43b5f9236f849ceac5a79066b3c1),
[`NetworkListTetherHostsHeaderView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_tether_hosts_header_view.h;l=16;drc=912e8ff344310668fae98ee2e41486045e675e3a),
and [`NetworkListWifiHeaderView`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_wifi_header_view.h;l=15;drc=869ec54af88f43b5f9236f849ceac5a79066b3c1)
classes all extend the `NetworkListNetworkHeaderView` class and implement additional
functionality that is specific to their section e.g., Cellular, Tether Hosts, or
WiFi.

## Controllers

The controller logic required by the network and VPN pages has been split into
different classes to improve testability. At a high level, we use one controller
to manage the entire page and we use a second controller to manage the list of
networks or VPNs within that page.

#### Hierarchy:
```
NetworkListViewController
 └─NetworkListViewControllerImpl

NetworkDetailedViewController
```

### NetworkListViewController

The
[`NetworkListViewController`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_view_controller.h;l=17;drc=3a215d1e60a3b32928a50d00ea07ae52ea491a16)
class defines the interface of the class responsible for managing the list of
networks within the network page. This class provides minimal APIs and exists to
improve testability; this class is instantiated using by the [`Factory`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_view_controller.h;l=19;drc=3a215d1e60a3b32928a50d00ea07ae52ea491a16)
class that it defines, allowing tests to use fake implementations of
`NetworkListviewController`. This class is implemented by
[`NetworkListViewControllerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_list_view_controller_impl.h;l=43;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
and contains all of the logic to ensure the list of networks and network
technologies in the network page are updated.

This class observes the network data model,
[`TrayNetworkStateModel`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/tray_network_state_model.h;l=28;drc=b8c7dcc70eebd36c4b68be590ca7b5654955002d),
and uses the information provided to update the network page. This class will
reorder views if possible and will ensure that any network changes are
propagated to the corresponding `NetworkListNetworkItemView`.

When new networks are added or new technologies become available, the
`NetworkListViewController` class will leverage the APIs provided by the
`NetworkDetailedNetworkView` class to instantiate headers or network items.

### NetworkDetailedViewController

The
[`NetworkDetailedViewController`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/network_detailed_view_controller.h;l=32;drc=deb7584e0d9e42e1e31d243735a4be5b630cb57b)
is responsible for the creation and management of the
`NetworkDetailedNetworkView` and `NetworkListViewController` classes. This class
implements the logic to handle networks being selected from the network list and
to handle technologies being enabled or disabled.

