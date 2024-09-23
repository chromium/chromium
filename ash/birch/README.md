# Birch

B.I.R.C.H. stands for `B`uilt `I`n `R`ecommendations for `CH`romeOS.

Birch is the system which fetches, stores, and displays suggestion chips as part
of informed restore as well as overview mode of the ChromeOS System UI.

The UX goal is to get the user back into a task that they might be interested in
after pausing their session for some reason.

## Suggestion Types

The following is a list of suggestions types that can be shown by birch UI.

- Calendar events
- Calendar file attachments
- Recent tabs from another device
- Recent Drive files
- Last active tab
- Most visited tab
- Self Share (Tab shared from another device)
- Lost Media (Tab with media currently playing)
- Release notes
- Weather

The user can customize which suggestion types are shown via context menu.

## Data Flow

When birch data is needed, a data fetch request is sent to the
[`BirchModel`](/ash/birch/birch_model.h) via ` RequestBirchDataFetch()`. The
`BirchModel` will then request data from each
[`BirchDataProvider`](/ash/birch/birch_data_provider.h). Data providers then
send birch items back to the model to be stored. Once all items have been
fetched, or the data fetch timeout has expired, the requester is notified. At
this point the requester can get the top items from the model to display in the
UI via `GetItemsForDisplay()`.

Many data providers fetch data utilizing the user's Chrome browser profile, and
so are created and owned by the
[`BirchKeyedService`](/chrome/browser/ui/ash/birch/birch_keyed_service.cc) in
`/chrome/browser/ui/ash/birch`.

In the UI, a birch suggestion is displayed as a
[`BirchChipButton`](/ash/wm/overview/birch/birch_chip_button.cc).

## Ranker

The [`BirchRanker`](/ash/birch/birch_ranker.h) assigns a numeric rank to each
`BirchItem` for ordering in the system UI. The top four items are chosen for
display in the UI.

## Item Remover

The [`BirchItemRemover`](/ash/birch/birch_item_remover.h) will remove and keep
track of items specifically hidden by the user via the `BirchChipButton`'s
context menu. These items will not be shown to the user again.

The following item types cannot be removed by the item remover. Weather and lost
media items can instead be hidden by customizing the shown suggestion types
using the birch context menu.

- Release Notes
- Weather
- Lost Media