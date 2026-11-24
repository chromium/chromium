// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.chromium.build.location_rewriter.LocationRewriter;

import java.util.Arrays;

/** Single entry point dispatching to all bytecode rewriter tools. */
public class BytecodeRewriterMain {
    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.err.println(
                    "Usage: bytecode_rewriter"
                            + " <location_rewriter|system_load_library_remover|trace_event_adder>"
                            + " [args...]");
            System.exit(2);
        }
        String[] toolArgs = Arrays.copyOfRange(args, 1, args.length);
        switch (args[0]) {
            case "location_rewriter":
                LocationRewriter.main(toolArgs);
                break;
            case "system_load_library_remover":
                SystemLoadLibraryRemover.main(toolArgs);
                break;
            case "trace_event_adder":
                TraceEventAdder.main(toolArgs);
                break;
            default:
                System.err.println("Unknown tool: " + args[0]);
                System.exit(2);
        }
    }
}
