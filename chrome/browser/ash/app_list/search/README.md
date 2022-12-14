# About

This folder contains the backend implementation of Chrome OS launcher search.

# Overview of search infrastructure

## Important classes

### Core

- **SearchController**. This controls all the core search functions such as
  starting a search, collecting results, ranking and publishing. Implemented by
  [`SearchControllerImplNew`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller_impl.h;l=44;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571).

  To interact with the frontend, it calls the 
  [`AppListController`](https://source.chromium.org/chromium/chromium/src/+/main:ash/public/cpp/app_list/app_list_controller.h;l=31;drc=16b9100fa38b90f93e29fb6d7e4578a7eaeb7a1f) and
  [`AppListModelUpdater`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/app_list_model_updater.h;l=26;drc=4a8573cb240df29b0e4d9820303538fb28e31d84), 
  and is called by the [`AppListClient`](https://source.chromium.org/chromium/chromium/src/+/main:ash/public/cpp/app_list/app_list_client.h;l=36;drc=3a215d1e60a3b32928a50d00ea07ae52ea491a16).
- **SearchProvider**. The base class for all search providers. Each search
  provider typically handles one type of result, such as settings, apps or
  files. Some search providers implement their search function locally, while
  others call out to further backends.
- **SearchControllerFactory**. Responsible for the creation of the search
  controller and its providers at start-up time.
- **ChromeSearchResult**. The base class for all search results. Each
  [`ChromeSearchResult`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/chrome_search_result.h;l=35;drc=f828fc7710b7922a4339c030da3cfe48497d4300) 
  contains the information associated with one result. This information is stored in a 
  [`SearchResultMetadata`](https://source.chromium.org/chromium/chromium/src/+/main:ash/public/cpp/app_list/app_list_types.h;l=571;drc=180c7396abb3e4aa0a020babde5b19e80035ca43) 
  object which is piped to the frontend code.

### Ranking

Ranking is the process of assigning scores to each result and category to
determine their final display order. Located inside the 
[`ranking/`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/ranking/) 
subdirectory.

- **RankerManager**. This owns the ranking stack and determines the order of
  ranking steps.
- **Ranker**. The base class for all rankers. Rankers can be used for all kinds
  of post-processing steps, including but not limited to ranking.

### Metrics

- **AppListNotifierImpl**. Located in the parent directory
  [`chrome/browser/ash/app_list/`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/). 
  Contains a state machine that converts raw UI events into information such as impressions and launches.
- **SearchMetricsManager**. Observes the [`AppListNotifier`](https://source.chromium.org/chromium/chromium/src/+/main:ash/public/cpp/app_list/app_list_notifier.h;l=28;drc=ccc5ecdf824f172bf8675eb33f5377483289c334)
  and logs metrics accordingly.

## Life of a search query

1. The user types a query into the launcher search box. This filters through UI
   code until it eventually reaches 
   [`SearchController::StartSearch(query)`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=70;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571).
2. The [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) 
  forwards this query to its various search providers.
3. Search providers return their results **asynchronously**.
4. The [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) 
collects these results and performs ranking on the results and their categories.
5. Results are published to the UI.

Steps #3-5 may be repeated several times due to the asynchronous nature of #3.
The [`BurnInController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/burnin_controller.h;l=20;drc=f828fc7710b7922a4339c030da3cfe48497d4300) 
contains timing logic to reduce the UI effect of results popping in.

Training may be performed:

6. The user clicks on a result.
7. The [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) 
forwards this information to its various search providers and rankers, 
which can use this information to inform future searches and ranking.

## Life of zero state

Zero state is the UI shown before the user types any query. It consists of the
Continue section (recent files), the recent apps row, as well as the app grid.
The [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571)
handles ranking for continue files and recent apps.

Steps #1-4 closely mirror query search, but publishing is handled differently.

1. The user opens the launcher. This eventually reaches
   [`SearchController::StartZeroState(callback, timeout)`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=72;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571).
   - The UI blocks itself until `callback` is run, which by contract should
     happen no later than `timeout`.
2. The [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) 
  forwards this request to its various zero state providers.
3. Providers return their results **asynchronously**.
4. The [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) 
  collects these results and performs ranking on the results and their categories.
5. Once either of the following two conditions is satisfied, the
   [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) 
   will publish any existing results and unblock the UI:
   - [`timeout`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=73;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) has elapsed,
   - All zero state providers have returned.
6. If there are any providers still pending, the [`SearchController`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/app_list/search/search_controller.h;l=50;drc=ec05d2cd9ff57132c80e7071942626f98c6e3571) waits until
   all of them have returned and publishes results once more to the UI.

The most common situation is that recent apps return before the timeout, but the
continue files providers return later.

Training may be performed, the same as with query search.
