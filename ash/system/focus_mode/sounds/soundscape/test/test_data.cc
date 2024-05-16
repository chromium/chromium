// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/test/test_data.h"

namespace ash {

constexpr char kTestConfig[] = R"(
{
  "version": "20240426",
  "playlists": [
    {
      "name": [
        {
          "locale": "en-US",
          "name": "Classical"
        },
        {
          "locale": "fr-CA",
          "name": "Musique Classique"
        }
      ],
      "uuid": "a58d4418-c835-4254-b5d6-cca909046202",
      "thumbnail": "/thumbnails/classical_20240426.png",
      "tracks": [
        {
          "name": "Violins",
          "artist": "Artist",
          "path": "/tracks/violins_20240426.mp3"
        },
        {
          "name": "Cello",
          "artist": "Artist",
          "path": "/tracks/cello_20240426.mp3"
        }
      ]
    },
    {
      "name": [
        {
          "locale": "en-US",
          "name": "Nature"
        },
        {
          "locale": "fr-CA",
          "name": "Nature"
        }
      ],
      "uuid": "1c25911f-d847-4500-94e3-6e2d9a52846a",
      "thumbnail": "/thumbnails/nature_20240426.png",
      "tracks": [
        {
          "name": "Bears",
          "artist": "Artist",
          "path": "/tracks/bears_20240426.mp3"
        },
        {
          "name": "Penguins",
          "artist": "Artist",
          "path": "/tracks/penguins_20240426.mp3"
        }
      ]
    },
    {
      "name": [
        {
          "locale": "en-US",
          "name": "Flow"
        },
        {
          "locale": "fr-CA",
          "name": "Flux"
        }
      ],
      "uuid": "921c4f5c-ccd8-4f1a-ba19-0b3c2685aa92",
      "thumbnail": "/thumbnails/flow_20240426.png",
      "tracks": [
        {
          "name": "A Longer Song Name",
          "artist": "Artist",
          "path": "/tracks/track_20240426.mp3"
        },
        {
          "name": "Release",
          "artist": "Artist",
          "path": "/tracks/release_20240426.mp3"
        }
      ]
    },
    {
      "name": [
        {
          "locale": "en-US",
          "name": "Ambiance"
        },
        {
          "locale": "fr-CA",
          "name": "Ambiance"
        }
      ],
      "uuid": "1b12ce3a-4857-4aae-95df-65cef1c4d4f3",
      "thumbnail": "/thumbnails/ambiance_20240426.png",
      "tracks": [
        {
          "name": "Rain",
          "artist": "Artist",
          "path": "/tracks/rain_20240426.mp3"
        },
        {
          "name": "Coffee Shop",
          "artist": "Artist",
          "path": "/tracks/coffee_shop_20240426.mp3"
        }
      ]
    }
  ]
}
)";

}  // namespace ash
