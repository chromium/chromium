# analytics.js

* [Project Page](https://developers.google.com/analytics/devguides/collection/analyticsjs)
* The extern file [universal_analytics_api.js](https://github.com/google/closure-compiler/blob/4327b35e038666593b0c72f90e75c4f33fc7a060/contrib/externs/universal_analytics_api.js) is copied from the [closure compiler project](https://github.com/google/closure-compiler)

# comlink.js

* [Project Page](https://github.com/GoogleChromeLabs/comlink)
* The ES module build is get from [unpkg](https://unpkg.com/comlink@4.2.0/dist/esm/comlink.js) with minor Closure compiler fixes and reformatting.

# FFMpeg

[Project Page](https://www.ffmpeg.org/)

Follow the [Emscripten Getting Started Instruction](https://emscripten.org/docs/getting_started/downloads.html) to setup the toolchain. In short:

```shell
$ git clone https://github.com/emscripten-core/emsdk.git
$ cd emsdk
$ ./emsdk install latest
$ ./emsdk activate latest
$ source ./emsdk_env.sh
```

You can find the current used version from the output of `./emsdk activate latest` as:

```
Set the following tools as active:
   node-14.15.5-64bit
   releases-upstream-7b3cd38017f7c582cfa3ac24a9f12aa6a8dca51f-64bit
```

After the Emscripten environment is setup properly, run `build_ffmpeg.sh` will build `ffmpeg.{js,wasm}` from `src/third_party/ffmpeg`.

The emsdk version of the last build of this package is 1.39.13. TODO(b/199980849): Upgrade the version to 2.0.XX.
