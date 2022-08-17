// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_EXPORT_H_
#define BASE_BASE_EXPORT_H_

// 符号导出(elf文件)编译器跨平台支持，主要是win和其他(其实主要就是Unix/Linux)
// base_export.h：主要用以对不同的平台定义了导入和导出库相关的宏: BASE_EXPORT, 该类宏将贯穿整个base库。

// COMPONENT_BUILD宏：构建组件，只有需要构建组件时，才需要导出符号，源码编译不需要导出符号
// 其定义位于：build/config/compiler/BUILD.gn 中，如下：
// if (is_component_build) {
//   defines = [ "COMPONENT_BUILD" ]
// }
// 即当是组件build时，通过gn增加宏定义：COMPONENT_BUILD，进而在源代码中使用这个宏
#if defined(COMPONENT_BUILD)
#if defined(WIN32)
// BASE_IMPLEMENTATION宏：位于 base/BUILD.gn，如下：
// config("base_implementation") {
//   defines = [ "BASE_IMPLEMENTATION" ] // 这里表示增加宏定义
//   configs = [ "//build/config/compiler:wexit_time_destructors" ]
// }
#if defined(BASE_IMPLEMENTATION)
#define BASE_EXPORT __declspec(dllexport)
#else
// 使用 __declspec(dllimport) 导入函数调用，可以更快地对调用添加批注。
// 访问导出的 DLL 数据始终需要 __declspec(dllimport)。
// 使用 __declspec(dllimport) 将 DLL 中的函数调用导入到应用程序中。
// 使用 __declspec(dllimport) 更好的原因是：因为链接器在不需要时不会生成 thunk。
// Thunk 使代码变大（在 RISC 系统上，它可以是多个指令），并可能降低缓存性能。如果
// 告诉编译器函数位于DLL中，则可以为你生成间接调用。
#define BASE_EXPORT __declspec(dllimport)
#endif  // defined(BASE_IMPLEMENTATION)
#else   // defined(WIN32)
#if defined(BASE_IMPLEMENTATION)
#define BASE_EXPORT __attribute__((visibility("default")))
#else
#define BASE_EXPORT
#endif  // defined(BASE_IMPLEMENTATION)
#endif
#else  // defined(COMPONENT_BUILD)
#define BASE_EXPORT
#endif

#endif  // BASE_BASE_EXPORT_H_

// 是时候总结一下__declspec(dllimport)的作用了。注意：这是个win概念，仅支持window编译器。
// 可能有人会问：__declspec(dllimport)和__declspec(dllexport)是一对的，在动态链接库中
// __declspec(dllexport)管导出，__declspec(dllimport)管导入，就像一个国家一样，有出口也有进口，
// 有什么难理解的呢？这是一种很自然的思路，开始我也是这样理解，但实际不对, dllimport 作用总结如下：
// 1. 在导入动态链接库中的全局变量方面起作用，且比extern更有效
// 2. __declspec(dllimport)的作用主要体现在导出类的静态成员方面，使弱符号能够在其他dll或调用该
//    dll库的应用程序中使用，即使static弱符号能够在外部被使用
// 3. dllimport让编译器能够区分函数符号是一个普通函数，还是来自于其他dll库，从而更好的优化。
