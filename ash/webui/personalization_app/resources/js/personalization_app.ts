// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview the main entry point for the Personalization SWA. This imports
 * all of the necessary global modules and polymer elements to bootstrap the
 * page.
 */

import '/strings.m.js';
import './ambient/album_list_element.js';
import './ambient/albums_subpage_element.js';
import './ambient/ambient_theme_item_element.js';
import './ambient/ambient_theme_list_element.js';
import './ambient/art_album_dialog_element.js';
import './ambient/ambient_duration_element.js';
import './ambient/ambient_preview_large_element.js';
import './ambient/ambient_preview_small_element.js';
import './ambient/ambient_subpage_element.js';
import './ambient/ambient_weather_element.js';
import './ambient/toggle_row_element.js';
import './ambient/topic_source_item_element.js';
import './ambient/topic_source_list_element.js';
import './ambient/zero_state_element.js';
import './keyboard_backlight/color_icon_element.js';
import './keyboard_backlight/color_selector_element.js';
import './keyboard_backlight/keyboard_backlight_element.js';
import './keyboard_backlight/zone_customization_element.js';
import './personalization_router_element.js';
import './personalization_test_api.js';
import './personalization_toast_element.js';
import './personalization_breadcrumb_element.js';
import './personalization_main_element.js';
import './theme/color_scheme_icon_svg_element.js';
import './theme/personalization_theme_element.js';
import './theme/dynamic_color_element.js';
import './time_of_day_banner_element.js';
import './user/avatar_camera_element.js';
import './user/avatar_list_element.js';
import './user/user_preview_element.js';
import './user/user_subpage_element.js';
import './utils.js';
import './wallpaper/index.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {isPersonalizationJellyEnabled} from './load_time_booleans.js';
import {emptyState} from './personalization_state.js';
import {PersonalizationStore} from './personalization_store.js';

/**
 * With optimize_webui, the generated JS files are bundled into single file
 * `chrome://personalization/js/personalization_app.js`. These exports are
 * necessary so they can be imported in tests.
 */
export {AlbumListElement} from './ambient/album_list_element.js';
export {AlbumsSubpageElement} from './ambient/albums_subpage_element.js';
export {AmbientActionName, AmbientActions, SetAlbumsAction, setAlbumsAction, SetAlbumSelectedAction, setAlbumSelectedAction, SetAmbientModeEnabledAction, setAmbientModeEnabledAction, SetAmbientThemeAction, setAmbientThemeAction, SetPreviewsAction, setPreviewsAction, SetScreenSaverDurationAction, setScreenSaverDurationAction, SetShouldShowTimeOfDayBannerAction, SetTemperatureUnitAction, setTemperatureUnitAction, SetTopicSourceAction, setTopicSourceAction} from './ambient/ambient_actions.js';
export {setAmbientProviderForTesting} from './ambient/ambient_interface_provider.js';
export {AmbientObserver} from './ambient/ambient_observer.js';
export {AmbientPreviewLargeElement} from './ambient/ambient_preview_large_element.js';
export {AmbientPreviewSmallElement} from './ambient/ambient_preview_small_element.js';
export {AmbientSubpageElement} from './ambient/ambient_subpage_element.js';
export {AmbientThemeItemElement} from './ambient/ambient_theme_item_element.js';
export {AmbientThemeListElement} from './ambient/ambient_theme_list_element.js';
export {AmbientWeatherUnitElement} from './ambient/ambient_weather_element.js';
export {ArtAlbumDialogElement} from './ambient/art_album_dialog_element.js';
export {ToggleRowElement} from './ambient/toggle_row_element.js';
export {TopicSourceItemElement} from './ambient/topic_source_item_element.js';
export {TopicSourceListElement} from './ambient/topic_source_list_element.js';
export {AmbientZeroStateElement} from './ambient/zero_state_element.js';
export {ColorIconElement} from './keyboard_backlight/color_icon_element.js';
export {KeyboardBacklightActionName, KeyboardBacklightActions, SetCurrentBacklightStateAction, setCurrentBacklightStateAction, SetShouldShowNudgeAction, setShouldShowNudgeAction, SetWallpaperColorAction, setWallpaperColorAction} from './keyboard_backlight/keyboard_backlight_actions.js';
export {KeyboardBacklightElement} from './keyboard_backlight/keyboard_backlight_element.js';
export {setKeyboardBacklightProviderForTesting} from './keyboard_backlight/keyboard_backlight_interface_provider.js';
export {KeyboardBacklightObserver} from './keyboard_backlight/keyboard_backlight_observer.js';
export {ZoneCustomizationElement} from './keyboard_backlight/zone_customization_element.js';
export {Actions, DismissErrorAction, dismissErrorAction, PersonalizationActionName, SetErrorAction} from './personalization_actions.js';

export * from '../personalization_app.mojom-webui.js';
export * from '../sea_pen.mojom-webui.js';
export {ColorScheme} from '../color_scheme.mojom-webui.js';
export {PersonalizationBreadcrumbElement} from './personalization_breadcrumb_element.js';
export {PersonalizationMainElement} from './personalization_main_element.js';
export {reduce} from './personalization_reducers.js';
export {Paths, PersonalizationRouterElement, QueryParams, ScrollableTarget} from './personalization_router_element.js';
export {emptyState, PersonalizationState} from './personalization_state.js';
export {PersonalizationStore} from './personalization_store.js';
export {PersonalizationThemeElement} from './theme/personalization_theme_element.js';
export {PersonalizationToastElement} from './personalization_toast_element.js';
export {setDarkModeEnabledAction, SetDarkModeEnabledAction, setColorSchemeAction, setStaticColorAction, SetStaticColorAction, SetSampleColorSchemesAction, SetColorSchemeAction, ThemeActionName, ThemeActions} from './theme/theme_actions.js';
export {setThemeProviderForTesting} from './theme/theme_interface_provider.js';
export {ColorSchemeIconSvgElement} from './theme/color_scheme_icon_svg_element.js';
export {DynamicColorElement} from './theme/dynamic_color_element.js';
export {getThemeProvider} from './theme/theme_interface_provider.js';
export {ThemeObserver} from './theme/theme_observer.js';
export {TimeOfDayBannerElement} from './time_of_day_banner_element.js';
export {AvatarCameraElement, AvatarCameraMode} from './user/avatar_camera_element.js';
export {AvatarListElement} from './user/avatar_list_element.js';
export {UserActionName} from './user/user_actions.js';
export {UserImageObserver} from './user/user_image_observer.js';
export {setUserProviderForTesting} from './user/user_interface_provider.js';
export {UserPreviewElement} from './user/user_preview_element.js';
export {UserSubpageElement} from './user/user_subpage_element.js';
export {GetUserMediaProxy, getWebcamUtils, setWebcamUtilsForTesting} from './user/webcam_utils_proxy.js';
export {getCountText, getNumberOfGridItemsPerRow, staticColorIds} from './utils.js';
export {DefaultImageSymbol, DisplayableImage, kDefaultImageSymbol, kMaximumLocalImagePreviews} from './wallpaper/constants.js';
export {GooglePhotosAlbumsElement} from './wallpaper/google_photos_albums_element.js';
export {GooglePhotosCollectionElement, GooglePhotosTab} from './wallpaper/google_photos_collection_element.js';
export {GooglePhotosPhotosByAlbumIdElement} from './wallpaper/google_photos_photos_by_album_id_element.js';
export {GooglePhotosPhotosElement, GooglePhotosPhotosRow, GooglePhotosPhotosSection} from './wallpaper/google_photos_photos_element.js';
export {GooglePhotosSharedAlbumDialogElement, AcceptEvent} from './wallpaper/google_photos_shared_album_dialog_element.js';
export {GooglePhotosZeroStateElement} from './wallpaper/google_photos_zero_state_element.js';
export {DEFAULT_COLOR_SCHEME} from './theme/utils.js';
export {LocalImagesElement} from './wallpaper/local_images_element.js';
export {RecentSeaPenData} from './wallpaper/sea_pen/constants.js';
export * from './wallpaper/sea_pen/sea_pen_actions.js';
export {getRecentSeaPenImages, selectRecentSeaPenImage, searchSeaPenThumbnails} from './wallpaper/sea_pen/sea_pen_controller.js';
export {SeaPenImagesElement} from './wallpaper/sea_pen/sea_pen_images_element.js';
export {SeaPenInputQueryElement} from './wallpaper/sea_pen/sea_pen_input_query_element.js';
export {SeaPenRecentWallpapersElement} from './wallpaper/sea_pen/sea_pen_recent_wallpapers_element.js';
export {SeaPenState} from './wallpaper/sea_pen/sea_pen_state.js';
export {SeaPenTemplateQueryElement} from './wallpaper/sea_pen/sea_pen_template_query_element.js';
export {setSeaPenProviderForTesting} from './wallpaper/sea_pen/sea_pen_interface_provider.js';
export {SparklePlaceholderElement} from './wallpaper/sea_pen/sparkle_placeholder_element.js';
export {isDefaultImage, isFilePath, isGooglePhotosPhoto, isWallpaperImage} from './wallpaper/utils.js';
export * from './wallpaper/wallpaper_actions.js';
export {WallpaperCollectionsElement} from './wallpaper/wallpaper_collections_element.js';
export {selectGooglePhotosAlbum, cancelPreviewWallpaper, confirmPreviewWallpaper, fetchCollections, fetchGooglePhotosAlbum, fetchGooglePhotosAlbums, fetchGooglePhotosPhotos, fetchGooglePhotosSharedAlbums, fetchLocalData, getDefaultImageThumbnail, getLocalImages, initializeBackdropData, fetchGooglePhotosEnabled, selectWallpaper, setCurrentWallpaperLayout, setDailyRefreshCollectionId, updateDailyRefreshWallpaper} from './wallpaper/wallpaper_controller.js';
export {WallpaperErrorElement} from './wallpaper/wallpaper_error_element.js';
export {WallpaperFullscreenElement} from './wallpaper/wallpaper_fullscreen_element.js';
export {WallpaperGridItemElement} from './wallpaper/wallpaper_grid_item_element.js';
export {getImageTiles, WallpaperImagesElement} from './wallpaper/wallpaper_images_element.js';
export {setWallpaperProviderForTesting} from './wallpaper/wallpaper_interface_provider.js';
export {WallpaperObserver} from './wallpaper/wallpaper_observer.js';
export {WallpaperPreviewElement} from './wallpaper/wallpaper_preview_element.js';
export {WallpaperSelectedElement} from './wallpaper/wallpaper_selected_element.js';
export {WallpaperSubpageElement} from './wallpaper/wallpaper_subpage_element.js';
export {WallpaperSubpageTopElement} from './wallpaper/wallpaper_subpage_top_element.js';
export {DailyRefreshType} from './wallpaper/wallpaper_state.js';
export {TimeOfDayAcceptEvent, TimeOfDayWallpaperDialogElement} from './wallpaper/time_of_day_wallpaper_dialog_element.js';

PersonalizationStore.getInstance().init(emptyState());
const link = document.querySelector('link[rel=\'icon\']') as HTMLLinkElement;
if (link) {
  // |link| may be null in tests.
  link.href = '/hub_icon_192.png';
}
document.title = loadTimeData.getString('personalizationTitle');

if (isPersonalizationJellyEnabled()) {
  // After the Jelly experiment is launched, add the link directly to
  // `index.html`.
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://theme/colors.css?sets=legacy,sys';
  document.head.appendChild(link);
  const fontLink = document.createElement('link');
  fontLink.rel = 'stylesheet';
  fontLink.href = 'chrome://theme/typography.css';
  document.head.appendChild(fontLink);
  document.body.classList.add('jelly-enabled');
  ColorChangeUpdater.forDocument().start();
}
