#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages Chromium's bytecode rewriters as a fat jar + GraalVM binary.

Bundles trace_event_adder, location_rewriter, and system_load_library_remover
into a single executable (bytecode_rewriter) dispatched by leading subcommand,
alongside bytecode_rewriter.jar for non-linux-x64 hosts.
"""

import os
import pathlib
import sys
import zipfile

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(
    p for p in _THIS_DIR.parents if (p / 'build' / '3pp_common').is_dir()
)
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common  # noqa: E402
import graalvm  # noqa: E402

_JAVA_SRC_DIR = 'build/android/bytecode/java'
# Path within the checkout. Cannot be under build/android/bytecode, since the
# 3pp recipe copies the package definition (this directory) there after
# "checkout" runs, and fails if it already exists.
_JAVA_CHECKOUT_SUBPATH = 'bytecode_java'
_ASM_JAR = 'third_party/android_deps/cipd/libs/org_ow2_asm_asm/asm.jar'
_MAIN_CLASS = 'org.chromium.bytecode.BytecodeRewriterMain'

_STUB_SOURCES = {
    'android/graphics/Canvas.java': (
        'package android.graphics;\npublic class Canvas {}\n'
    ),
    'android/view/MotionEvent.java': (
        'package android.view;\npublic class MotionEvent {}\n'
    ),
    'android/view/View.java': """\
package android.view;

import android.graphics.Canvas;

public class View {
  public boolean dispatchTouchEvent(MotionEvent event) { return false; }
  public void draw(Canvas canvas) {}
  protected void onMeasure(int width, int height) {}
  protected void onLayout(boolean changed, int l, int t, int r, int b) {}
}
""",
    'android/animation/Animator.java': """\
package android.animation;

public class Animator {
  public interface AnimatorListener {
    void onAnimationStart(Animator animation);
    void onAnimationEnd(Animator animation);
  }
}
""",
    'android/animation/ValueAnimator.java': """\
package android.animation;

public class ValueAnimator extends Animator {
  public interface AnimatorUpdateListener {
    void onAnimationUpdate(ValueAnimator animation);
  }
}
""",
    'org/chromium/base/task/Location.java': """\
package org.chromium.base.task;

public class Location {
  public static Location from(String file, String func, int line) {
    return new Location();
  }
}
""",
    'org/chromium/base/task/TaskRunner.java': """\
package org.chromium.base.task;

public interface TaskRunner {
  void postDelayedTask(Runnable task, long delay);
  void postDelayedTask(Runnable task, long delay, Location location);
}
""",
    'org/chromium/base/task/PostTask.java': """\
package org.chromium.base.task;

public class PostTask {
  public static void postTask(int traits, Runnable task) {}
  public static void postTask(int traits, Runnable task, Location location) {}
}
""",
}

_SAMPLE_APP_SOURCES = {
    'org/chromium/sample/BaseSampleView.java': """\
package org.chromium.sample;

import android.view.View;

public class BaseSampleView extends View {}
""",
    'org/chromium/sample/SampleView.java': """\
package org.chromium.sample;

import android.graphics.Canvas;

public class SampleView extends BaseSampleView {
  @Override
  public void draw(Canvas canvas) {
    if (canvas == null) {
      RuntimeException e = System.nanoTime() == 0
          ? new IllegalArgumentException("null")
          : new IllegalStateException("null");
      throw e;
    }
    super.draw(canvas);
  }
}
""",
    'org/chromium/sample/SampleListener.java': """\
package org.chromium.sample;

import android.animation.Animator;
import android.animation.ValueAnimator;

public class SampleListener
    implements Animator.AnimatorListener, ValueAnimator.AnimatorUpdateListener {
  @Override
  public void onAnimationStart(Animator a) {}
  @Override
  public void onAnimationEnd(Animator a) {}
  @Override
  public void onAnimationUpdate(ValueAnimator a) {}
}
""",
    'org/chromium/sample/SampleCaller.java': """\
package org.chromium.sample;

import android.view.View;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;

public class SampleCaller {
  public static Object run(boolean flag, TaskRunner runner, Runnable r) {
    // Merge app/SDK types and JDK types so COMPUTE_FRAMES exercises both jar
    // and jrt:/ hierarchy lookups.
    View v = flag ? new SampleView() : new BaseSampleView();
    RuntimeException ex = flag
        ? new IllegalArgumentException("a")
        : new IllegalStateException("b");
    PostTask.postTask(1, r);
    runner.postDelayedTask(r, 10L);
    return flag ? v : ex;
  }
}
""",
    'org/chromium/sample/SampleNativeLoader.java': """\
package org.chromium.sample;

import android.view.View;

public class SampleNativeLoader {
  public static View load(boolean flag) {
    View v = flag ? new SampleView() : new BaseSampleView();
    System.loadLibrary("sample_native");
    return v;
  }
}
""",
}


def _write_sources(src_dir, sources_dict):
    paths = []
    for rel_path, content in sorted(sources_dict.items()):
        full_path = pathlib.Path(src_dir) / rel_path
        full_path.parent.mkdir(parents=True, exist_ok=True)
        full_path.write_text(content, encoding='utf-8')
        paths.append(str(full_path))
    return paths


def _zip_dir(src_dir, dest_jar, *, extra_jars=(), manifest=None):
    seen = set()
    with zipfile.ZipFile(dest_jar, 'w', zipfile.ZIP_DEFLATED) as out_z:
        if manifest:
            out_z.writestr('META-INF/MANIFEST.MF', manifest)
            seen.add('META-INF/MANIFEST.MF')
        src_path = pathlib.Path(src_dir)
        for path in sorted(src_path.rglob('*')):
            if path.is_file():
                rel = path.relative_to(src_path).as_posix()
                if rel not in seen:
                    out_z.write(str(path), rel)
                    seen.add(rel)
        for jar_path in extra_jars:
            with zipfile.ZipFile(jar_path) as in_z:
                for info in in_z.infolist():
                    name = info.filename
                    if (
                        info.is_dir()
                        or name.startswith('META-INF/')
                        or name == 'module-info.class'
                        or name in seen
                    ):
                        continue
                    out_z.writestr(name, in_z.read(info))
                    seen.add(name)


def _read_jar_contents(jar_path):
    contents = {}
    with zipfile.ZipFile(jar_path) as z:
        for info in z.infolist():
            if not info.is_dir():
                contents[info.filename] = z.read(info)
    return contents


def _compile_sample_jars(javac, work_dir):
    """Builds stub SDK jar and sample app jar covering all three rewriters."""
    stub_src_dir = os.path.join(work_dir, 'stub_src')
    stub_classes_dir = os.path.join(work_dir, 'stub_classes')
    stub_sources = _write_sources(stub_src_dir, _STUB_SOURCES)
    common.run_cmd(
        [javac, '--release', '17', '-d', stub_classes_dir] + stub_sources
    )
    stub_jar = os.path.join(work_dir, 'stub_sdk.jar')
    _zip_dir(stub_classes_dir, stub_jar)

    app_src_dir = os.path.join(work_dir, 'app_src')
    app_classes_dir = os.path.join(work_dir, 'app_classes')
    app_sources = _write_sources(app_src_dir, _SAMPLE_APP_SOURCES)
    common.run_cmd(
        [javac, '--release', '17', '-g', '-cp', stub_jar, '-d', app_classes_dir]
        + app_sources
    )
    app_jar = os.path.join(work_dir, 'sample_app.jar')
    _zip_dir(app_classes_dir, app_jar)
    return stub_jar, app_jar


def _sample_commands(stub_jar, app_jar, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    loc_out = os.path.join(out_dir, 'sample_app.loc.jar')
    sllr_out = os.path.join(out_dir, 'sample_app.sllr.jar')
    sllr_no_init_out = os.path.join(out_dir, 'sample_app.sllr_no_init.jar')
    trace_out = os.path.join(out_dir, 'sample_app.trace.jar')
    trace_args_file = os.path.join(out_dir, 'trace_args.txt')
    pathlib.Path(trace_args_file).write_text(
        f'{app_jar}\n{trace_out}\n', encoding='utf-8'
    )
    return [
        (
            'location_rewriter',
            ['location_rewriter', '--classpath', stub_jar, app_jar, loc_out],
            loc_out,
            'org/chromium/sample/SampleCaller.class',
        ),
        (
            'system_load_library_remover_with_init',
            [
                'system_load_library_remover',
                '--classpath',
                stub_jar,
                app_jar,
                sllr_out,
                'org.chromium.androidx.SampleJniLoader',
            ],
            sllr_out,
            'org/chromium/sample/SampleNativeLoader.class',
        ),
        (
            'system_load_library_remover_no_init',
            [
                'system_load_library_remover',
                '--classpath',
                stub_jar,
                app_jar,
                sllr_no_init_out,
            ],
            sllr_no_init_out,
            'org/chromium/sample/SampleNativeLoader.class',
        ),
        (
            'trace_event_adder',
            [
                'trace_event_adder',
                '--classpath',
                stub_jar,
                f'@{trace_args_file}',
            ],
            trace_out,
            'org/chromium/sample/SampleView.class',
        ),
    ]


def _build_pgo_workloads(graal_home, stub_jar, app_jar, work_dir):
    """Creates representative multi-jar PGO workloads for all three tools."""
    javac = os.path.join(graal_home, 'bin', 'javac')
    jimage = os.path.join(graal_home, 'bin', 'jimage')
    pgo_dir = os.path.join(work_dir, 'pgo')
    os.makedirs(pgo_dir, exist_ok=True)

    # Extract jdk.compiler classes to give ASM and ZipInputStream /
    # ZipOutputStream a large corpus of real bytecode (~1,000 classes).
    mod_dir = os.path.join(pgo_dir, 'modules')
    common.run_cmd(
        [
            jimage,
            'extract',
            '--include',
            r'regex:/(jdk\.compiler|java\.compiler)/.*',
            '--dir',
            mod_dir,
            os.path.join(graal_home, 'lib', 'modules'),
        ]
    )
    compiler_jar = os.path.join(pgo_dir, 'jdk_compiler.jar')
    with zipfile.ZipFile(compiler_jar, 'w', zipfile.ZIP_DEFLATED) as z:
        for mod in ('jdk.compiler', 'java.compiler'):
            mod_root = pathlib.Path(mod_dir) / mod
            for p in sorted(mod_root.rglob('*.class')):
                if p.name != 'module-info.class':
                    z.write(str(p), p.relative_to(mod_root).as_posix())

    # Generate 24 synthetic classes split across 12 jars so PGO exercises
    # ClassHierarchy's multi-jar indexing and rewriting paths across many jars.
    syn_src = {}
    for i in range(24):
        syn_src[f'org/chromium/pgo/PgoView{i}.java'] = (
            f'package org.chromium.pgo;\n'
            f'import android.graphics.Canvas;\n'
            f'import org.chromium.sample.BaseSampleView;\n'
            f'public class PgoView{i} extends BaseSampleView {{\n'
            f'  @Override public void draw(Canvas c) {{\n'
            f'    if (c == null) return;\n'
            f'    super.draw(c);\n'
            f'  }}\n'
            f'}}\n'
        )
        syn_src[f'org/chromium/pgo/PgoCaller{i}.java'] = (
            f'package org.chromium.pgo;\n'
            f'import android.view.View;\n'
            f'import org.chromium.base.task.PostTask;\n'
            f'import org.chromium.base.task.TaskRunner;\n'
            f'public class PgoCaller{i} {{\n'
            f'  public static View call(boolean f, TaskRunner t, Runnable r)'
            f' {{\n'
            f'    View v = f ? new PgoView{i}() : new View();\n'
            f'    PostTask.postTask({i}, r);\n'
            f'    t.postDelayedTask(r, {i}L);\n'
            f'    System.loadLibrary("lib{i}");\n'
            f'    return v;\n'
            f'  }}\n'
            f'}}\n'
        )
    syn_src_dir = os.path.join(pgo_dir, 'syn_src')
    syn_classes_dir = os.path.join(pgo_dir, 'syn_classes')
    syn_paths = _write_sources(syn_src_dir, syn_src)
    common.run_cmd(
        [
            javac,
            '--release',
            '17',
            '-g',
            '-cp',
            os.pathsep.join([stub_jar, app_jar]),
            '-d',
            syn_classes_dir,
        ]
        + syn_paths
    )

    shard_in_jars = []
    shard_out_jars = []
    for shard in range(12):
        in_jar = os.path.join(pgo_dir, f'shard_{shard}.jar')
        out_jar = os.path.join(pgo_dir, f'shard_{shard}.traced.jar')
        with zipfile.ZipFile(in_jar, 'w', zipfile.ZIP_DEFLATED) as z:
            for j in range(shard * 2, shard * 2 + 2):
                for prefix in ('PgoView', 'PgoCaller'):
                    rel = f'org/chromium/pgo/{prefix}{j}.class'
                    z.write(os.path.join(syn_classes_dir, rel), rel)
        shard_in_jars.append(in_jar)
        shard_out_jars.append(out_jar)

    combined_pgo_jar = os.path.join(pgo_dir, 'combined_pgo.jar')
    _zip_dir(
        syn_classes_dir, combined_pgo_jar, extra_jars=[compiler_jar, app_jar]
    )

    trace_in_jars = [compiler_jar, app_jar] + shard_in_jars
    trace_out_jars = [
        os.path.join(pgo_dir, 'jdk_compiler.traced.jar'),
        os.path.join(pgo_dir, 'sample_app.traced.jar'),
    ] + shard_out_jars
    pgo_trace_args = os.path.join(pgo_dir, 'pgo_trace_args.txt')
    pathlib.Path(pgo_trace_args).write_text(
        ':'.join(trace_in_jars) + '\n' + ':'.join(trace_out_jars) + '\n',
        encoding='utf-8',
    )

    full_cp = ':'.join([stub_jar, app_jar] + shard_in_jars)
    return [
        [
            'location_rewriter',
            '--classpath',
            full_cp,
            combined_pgo_jar,
            os.path.join(pgo_dir, 'combined_pgo.loc.jar'),
        ],
        [
            'system_load_library_remover',
            '--classpath',
            full_cp,
            combined_pgo_jar,
            os.path.join(pgo_dir, 'combined_pgo.sllr.jar'),
            'org.chromium.androidx.SampleJniLoader',
        ],
        [
            'trace_event_adder',
            '--classpath',
            stub_jar,
            f'@{pgo_trace_args}',
        ],
    ]


def do_latest():
    # common.main() appends a hash of loaded Python modules, version_deps
    # (//build/android/bytecode/java/** and asm.jar), and
    # graalvm.version_strings (e.g. "1.<hash>"), so returning a constant
    # automatically triggers rebuilds whenever inputs or GraalVM change. Bump
    # manually only to force a rebuild without input changes.
    return '1'


def do_install(args):
    graal_home = graalvm.ensure_graalvm()
    javac = os.path.join(graal_home, 'bin', 'javac')

    java_dir = pathlib.Path(common.path_within_checkout(_JAVA_CHECKOUT_SUBPATH))
    java_files = sorted(str(p) for p in java_dir.rglob('*.java'))
    if not java_files:
        raise RuntimeError(f'No .java files found under {java_dir}')

    asm_jar = common.path_within_checkout(_ASM_JAR)
    classes_dir = os.path.abspath('bytecode_classes')
    os.makedirs(classes_dir, exist_ok=True)
    common.run_cmd(
        [
            javac,
            '--release',
            '17',
            '-Werror',
            '-Xlint:unchecked,deprecation',
            '-cp',
            asm_jar,
            '-d',
            classes_dir,
        ]
        + java_files
    )

    jar_path = os.path.join(args.output_prefix, 'bytecode_rewriter.jar')
    _zip_dir(
        classes_dir,
        jar_path,
        extra_jars=[asm_jar],
        manifest=f'Manifest-Version: 1.0\nMain-Class: {_MAIN_CLASS}\n',
    )

    work_dir = os.path.abspath('work')
    os.makedirs(work_dir, exist_ok=True)
    stub_jar, app_jar = _compile_sample_jars(javac, work_dir)
    trace_cmds = _sample_commands(
        stub_jar, app_jar, os.path.join(work_dir, 'trace_out')
    )
    pgo_args_list = _build_pgo_workloads(
        graal_home, stub_jar, app_jar, work_dir
    )

    binary_path = os.path.join(args.output_prefix, 'bytecode_rewriter')
    graalvm.build_native_image(
        jar_path,
        binary_path,
        main_class=_MAIN_CLASS,
        tracing_args_list=[cmd for _, cmd, _, _ in trace_cmds],
        pgo_args_list=pgo_args_list,
        check_workloads=True,
        extra_native_image_args=[
            '-H:+UnlockExperimentalVMOptions',
            '-H:+AllowJRTFileSystem',
        ],
    )

    # Smoke-test every subcommand against the JVM outputs produced by tracing.
    orig_app_contents = _read_jar_contents(app_jar)
    native_cmds = _sample_commands(
        stub_jar, app_jar, os.path.join(work_dir, 'smoke_native')
    )
    env = dict(os.environ, JAVA_HOME='/nonexistent')
    for (name, _, jvm_out, changed_class), (
        _,
        native_args,
        native_out,
        _,
    ) in zip(trace_cmds, native_cmds):
        common.run_cmd(
            [binary_path, f'-Djava.home={graal_home}'] + native_args, env=env
        )
        jvm_contents = _read_jar_contents(jvm_out)
        native_contents = _read_jar_contents(native_out)
        if jvm_contents != native_contents:
            raise RuntimeError(
                f'Smoke test mismatch between JVM and native binary for {name}'
            )
        if native_contents[changed_class] == orig_app_contents[changed_class]:
            raise RuntimeError(
                f'Smoke test for {name} did not modify {changed_class}'
            )


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
        runtime_deps=[
            ('//' + _JAVA_SRC_DIR, _JAVA_CHECKOUT_SUBPATH),
            '//' + _ASM_JAR,
        ],
        version_deps=[
            '//' + _JAVA_SRC_DIR,
            '//' + _ASM_JAR,
        ],
        version_strings_func=graalvm.version_strings,
    )


if __name__ == '__main__':
    main()
