// Copyright 2021 The Chromium Authors. All rights reserved.
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
import './ambient/animation_theme_item_element.js';
import './ambient/animation_theme_list_element.js';
import './ambient/art_album_dialog_element.js';
import './ambient/ambient_preview_element.js';
import './ambient/ambient_subpage_element.js';
import './ambient/ambient_weather_element.js';
import './ambient/toggle_row_element.js';
import './ambient/topic_source_item_element.js';
import './ambient/topic_source_list_element.js';
import './ambient/zero_state_element.js';
import './personalization_router_element.js';
import './personalization_test_api.js';
import './personalization_toast_element.js';
import './personalization_breadcrumb_element.js';
import './personalization_main_element.js';
import './personalization_theme_element.js';
import './user/avatar_camera_element.js';
import './user/avatar_list_element.js';
import './user/user_preview_element.js';
import './user/user_subpage_element.js';
import './wallpaper/index.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {emptyState} from './personalization_state.js';
import {PersonalizationStore} from './personalization_store.js';

/**
 * With optimize_webui, the generated JS files are bundled into single file
 * `chrome://personalization/trusted/personalization_app.js`. These exports are
 * necessary so they can be imported in tests.
 */
export {kMaximumGooglePhotosPreviews, kMaximumLocalImagePreviews} from '../common/constants.js';
export {getCountText, getNumberOfGridItemsPerRow} from '../common/utils.js';
export {CollectionsGrid} from '../untrusted/collections_grid.js';
export {ImagesGrid} from '../untrusted/images_grid.js';
export {AlbumList} from './ambient/album_list_element.js';
export {AlbumsSubpage} from './ambient/albums_subpage_element.js';
export {AmbientActionName, AmbientActions, SetAlbumsAction, setAlbumsAction, SetAlbumSelectedAction, setAlbumSelectedAction, SetAmbientModeEnabledAction, setAmbientModeEnabledAction, SetAnimationThemeAction, setAnimationThemeAction, SetTemperatureUnitAction, setTemperatureUnitAction, SetTopicSourceAction, setTopicSourceAction} from './ambient/ambient_actions.js';
export {setAmbientProviderForTesting} from './ambient/ambient_interface_provider.js';
export {AmbientObserver} from './ambient/ambient_observer.js';
export {AmbientPreview} from './ambient/ambient_preview_element.js';
export {AmbientSubpage} from './ambient/ambient_subpage_element.js';
export {AmbientWeatherUnit} from './ambient/ambient_weather_element.js';
export {AnimationThemeItem} from './ambient/animation_theme_item_element.js';
export {AnimationThemeList} from './ambient/animation_theme_list_element.js';
export {ArtAlbumDialog} from './ambient/art_album_dialog_element.js';
export {ToggleRow} from './ambient/toggle_row_element.js';
export {TopicSourceItem} from './ambient/topic_source_item_element.js';
export {TopicSourceList} from './ambient/topic_source_list_element.js';
export {AmbientZeroState} from './ambient/zero_state_element.js';
export {IFrameApi} from './iframe_api.js';
export {Actions, DismissErrorAction, dismissErrorAction, PersonalizationActionName} from './personalization_actions.js';
export {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverRemote, AmbientProviderInterface, AnimationTheme, CurrentWallpaper, DefaultUserImage, FetchGooglePhotosAlbumsResponse, FetchGooglePhotosPhotosResponse, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, OnlineImageType, TemperatureUnit, ThemeObserverInterface, ThemeObserverRemote, ThemeProviderInterface, TopicSource, UserImage, UserImageObserverInterface, UserImageObserverRemote, UserInfo, UserProviderInterface, WallpaperCollection, WallpaperImage, WallpaperLayout, WallpaperObserverInterface, WallpaperObserverRemote, WallpaperProviderInterface, WallpaperType} from './personalization_app.mojom-webui.js';
export {PersonalizationBreadcrumb} from './personalization_breadcrumb_element.js';
export {PersonalizationMain} from './personalization_main_element.js';
export {DisplayableImage, reduce} from './personalization_reducers.js';
export {Paths, PersonalizationRouter} from './personalization_router_element.js';
export {emptyState, PersonalizationState} from './personalization_state.js';
export {PersonalizationStore} from './personalization_store.js';
export {PersonalizationThemeElement} from './personalization_theme_element.js';
export {PersonalizationToastElement} from './personalization_toast_element.js';
export {setDarkModeEnabledAction, SetDarkModeEnabledAction, ThemeActionName, ThemeActions} from './theme/theme_actions.js';
export {setThemeProviderForTesting} from './theme/theme_interface_provider.js';
export {ThemeObserver} from './theme/theme_observer.js';
export {AvatarCamera, AvatarCameraMode} from './user/avatar_camera_element.js';
export {AvatarList} from './user/avatar_list_element.js';
export {UserActionName} from './user/user_actions.js';
export {UserImageObserver} from './user/user_image_observer.js';
export {setUserProviderForTesting} from './user/user_interface_provider.js';
export {UserPreview} from './user/user_preview_element.js';
export {UserSubpage} from './user/user_subpage_element.js';
export {GetUserMediaProxy, getWebcamUtils, setWebcamUtilsForTesting} from './user/webcam_utils_proxy.js';
export {GooglePhotosAlbums} from './wallpaper/google_photos_albums_element.js';
export {GooglePhotosCollection} from './wallpaper/google_photos_collection_element.js';
export {GooglePhotosPhotosByAlbumId} from './wallpaper/google_photos_photos_by_album_id_element.js';
export {GooglePhotosPhotos, GooglePhotosPhotosRow, GooglePhotosPhotosSection} from './wallpaper/google_photos_photos_element.js';
export {GooglePhotosZeroState} from './wallpaper/google_photos_zero_state_element.js';
export {LocalImages} from './wallpaper/local_images_element.js';
export * from './wallpaper/wallpaper_actions.js';
export {WallpaperCollections} from './wallpaper/wallpaper_collections_element.js';
export {cancelPreviewWallpaper, confirmPreviewWallpaper, fetchCollections, fetchGooglePhotosAlbum, fetchGooglePhotosAlbums, fetchGooglePhotosPhotos, fetchLocalData, getDailyRefreshCollectionId, getLocalImages, initializeBackdropData, initializeGooglePhotosData, selectWallpaper, setCurrentWallpaperLayout, setDailyRefreshCollectionId, updateDailyRefreshWallpaper} from './wallpaper/wallpaper_controller.js';
export {WallpaperError} from './wallpaper/wallpaper_error_element.js';
export {WallpaperFullscreen} from './wallpaper/wallpaper_fullscreen_element.js';
export {WallpaperGridItem} from './wallpaper/wallpaper_grid_item_element.js';
export {getDarkLightImageTiles, getRegularImageTiles, WallpaperImages} from './wallpaper/wallpaper_images_element.js';
export {setWallpaperProviderForTesting} from './wallpaper/wallpaper_interface_provider.js';
export {WallpaperObserver} from './wallpaper/wallpaper_observer.js';
export {WallpaperPreview} from './wallpaper/wallpaper_preview_element.js';
export {WallpaperSelected} from './wallpaper/wallpaper_selected_element.js';

PersonalizationStore.getInstance().init(emptyState());
const link = document.querySelector('link[rel=\'icon\']') as HTMLLinkElement;
if (loadTimeData.getBoolean('isPersonalizationHubEnabled')) {
  link!.href = '/hub_icon_192.png';
}
document.title = loadTimeData.getBoolean('isPersonalizationHubEnabled') ?
    loadTimeData.getString('personalizationTitle') :
    loadTimeData.getString('wallpaperLabel');
