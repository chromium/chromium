// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Java mirror of base/command_line.h.
 * Android applications don't have command line arguments. Instead, they're "simulated" by reading a
 * file at a specific location early during startup. Applications each define their own files, e.g.,
 * ContentShellApplication.COMMAND_LINE_FILE.
**/
@MainDex
public abstract class CommandLine {
    // Public abstract interface, implemented in derived classes.
    // All these methods reflect their native-side counterparts.
    /**
     *  Returns true if this command line contains the given switch.
     *  (Switch names ARE case-sensitive).
     */
    public abstract boolean hasSwitch(String switchString);

    /**
     * Return the value associated with the given switch, or null.
     * @param switchString The switch key to lookup. It should NOT start with '--' !
     * @return switch value, or null if the switch is not set or set to empty.
     */
    public abstract String getSwitchValue(String switchString);

    /**
     * Return the value associated with the given switch, or {@code defaultValue} if the switch
     * was not specified.
     * @param switchString The switch key to lookup. It should NOT start with '--' !
     * @param defaultValue The default value to return if the switch isn't set.
     * @return Switch value, or {@code defaultValue} if the switch is not set or set to empty.
     */
    public String getSwitchValue(String switchString, String defaultValue) {
        String value = getSwitchValue(switchString);
        return TextUtils.isEmpty(value) ? defaultValue : value;
    }

    /**
     * Return a copy of all switches, along with their values.
     */
    public abstract Map getSwitches();

    /**
     * Append a switch to the command line.  There is no guarantee
     * this action happens before the switch is needed.
     * @param switchString the switch to add.  It should NOT start with '--' !
     */
    public abstract void appendSwitch(String switchString);

    /**
     * Append a switch and value to the command line.  There is no
     * guarantee this action happens before the switch is needed.
     * @param switchString the switch to add.  It should NOT start with '--' !
     * @param value the value for this switch.
     * For example, --foo=bar becomes 'foo', 'bar'.
     */
    public abstract void appendSwitchWithValue(String switchString, String value);

    /**
     * Append switch/value items in "command line" format (excluding argv[0] program name).
     * E.g. { '--gofast', '--username=fred' }
     * @param array an array of switch or switch/value items in command line format.
     *   Unlike the other append routines, these switches SHOULD start with '--' .
     *   Unlike init(), this does not include the program name in array[0].
     */
    public abstract void appendSwitchesAndArguments(String[] array);

    /**
     * Remove the switch from the command line.  If no such switch is present, this has no effect.
     * @param switchString The switch key to lookup. It should NOT start with '--' !
     */
    public abstract void removeSwitch(String switchString);

    /**
     * Determine if the command line is bound to the native (JNI) implementation.
     * @return true if the underlying implementation is delegating to the native command line.
     */
    public boolean isNativeImplementation() {
        return false;
    }

    /**
     * Returns the switches and arguments passed into the program, with switches and their
     * values coming before all of the arguments.
     */
    protected abstract String[] getCommandLineArguments();

    /**
     * Destroy the command line. Called when a different instance is set.
     * @see #setInstance
     */
    protected void destroy() {}

    private static final AtomicReference<CommandLine> sCommandLine =
            new AtomicReference<CommandLine>();

    /**
     * @return true if the command line has already been initialized.
     */
    public static boolean isInitialized() {
        return sCommandLine.get() != null;
    }

    // Equivalent to CommandLine::ForCurrentProcess in C++.
    public static CommandLine getInstance() {
        CommandLine commandLine = sCommandLine.get();
        assert commandLine != null;
        return commandLine;
    }

    /**
     * Initialize the singleton instance, must be called exactly once (either directly or
     * via one of the convenience wrappers below) before using the static singleton instance.
     * @param args command line flags in 'argv' format: args[0] is the program name.
     */
    public static void init(@Nullable String[] args) {
        setInstance(new JavaCommandLine(args));
    }

    /**
     * Initialize the command line from the command-line file.
     *
     * @param file The fully qualified command line file.
     */
    public static void initFromFile(String file) {
        char[] buffer = readFileAsUtf8(file);
        init(buffer == null ? null : tokenizeQuotedArguments(buffer));
    }

    /**
     * Resets both the java proxy and the native command lines. This allows the entire
     * command line initialization to be re-run including the call to onJniLoaded.
     */
    @VisibleForTesting
    public static void reset() {
        setInstance(null);
    }

    /**
     * Parse command line flags from a flat buffer, supporting double-quote enclosed strings
     * containing whitespace. argv elements are derived by splitting the buffer on whitepace;
     * double quote characters may enclose tokens containing whitespace; a double-quote literal
     * may be escaped with back-slash. (Otherwise backslash is taken as a literal).
     * @param buffer A command line in command line file format as described above.
     * @return the tokenized arguments, suitable for passing to init().
     */
    @VisibleForTesting
    static String[] tokenizeQuotedArguments(char[] buffer) {
        // Just field trials can take up to 10K of command line.
        if (buffer.length > 64 * 1024) {
            // Check that our test runners are setting a reasonable number of flags.
            throw new RuntimeException("Flags file too big: " + buffer.length);
        }

        ArrayList<String> args = new ArrayList<String>();
        StringBuilder arg = null;
        final char noQuote = '\0';
        final char singleQuote = '\'';
        final char doubleQuote = '"';
        char currentQuote = noQuote;
        for (char c : buffer) {
            // Detect start or end of quote block.
            if ((currentQuote == noQuote && (c == singleQuote || c == doubleQuote))
                    || c == currentQuote) {
                if (arg != null && arg.length() > 0 && arg.charAt(arg.length() - 1) == '\\') {
                    // Last char was a backslash; pop it, and treat c as a literal.
                    arg.setCharAt(arg.length() - 1, c);
                } else {
                    currentQuote = currentQuote == noQuote ? c : noQuote;
                }
            } else if (currentQuote == noQuote && Character.isWhitespace(c)) {
                if (arg != null) {
                    args.add(arg.toString());
                    arg = null;
                }
            } else {
                if (arg == null) arg = new StringBuilder();
                arg.append(c);
            }
        }
        if (arg != null) {
            if (currentQuote != noQuote) {
                Log.w(TAG, "Unterminated quoted string: " + arg);
            }
            args.add(arg.toString());
        }
        return args.toArray(new String[args.size()]);
    }

    private static final String TAG = "CommandLine";
    private static final String SWITCH_PREFIX = "--";
    private static final String SWITCH_TERMINATOR = SWITCH_PREFIX;
    private static final String SWITCH_VALUE_SEPARATOR = "=";

    public static void enableNativeProxy() {
        // Make a best-effort to ensure we make a clean (atomic) switch over from the old to
        // the new command line implementation. If another thread is modifying the command line
        // when this happens, all bets are off. (As per the native CommandLine).
        sCommandLine.set(new NativeCommandLine(getJavaSwitchesOrNull()));
    }

    @Nullable
    public static String[] getJavaSwitchesOrNull() {
        CommandLine commandLine = sCommandLine.get();
        if (commandLine != null) {
            return commandLine.getCommandLineArguments();
        }
        return null;
    }

    private static void setInstance(CommandLine commandLine) {
        CommandLine oldCommandLine = sCommandLine.getAndSet(commandLine);
        if (oldCommandLine != null) {
            oldCommandLine.destroy();
        }
    }

    /**
     * Set {@link CommandLine} for testing.
     * @param commandLine The {@link CommandLine} to use.
     */
    @VisibleForTesting
    public static void setInstanceForTesting(CommandLine commandLine) {
        setInstance(commandLine);
    }

    /**
     * @param fileName the file to read in.
     * @return Array of chars read from the file, or null if the file cannot be read.
     */
    private static char[] readFileAsUtf8(String fileName) {
        File f = new File(fileName);
        try (FileReader reader = new FileReader(f)) {
            char[] buffer = new char[(int) f.length()];
            int charsRead = reader.read(buffer);
            // charsRead < f.length() in the case of multibyte characters.
            return Arrays.copyOfRange(buffer, 0, charsRead);
        } catch (IOException e) {
            return null; // Most likely file not found.
        }
    }

    private CommandLine() {}

    private static class JavaCommandLine extends CommandLine {
        private HashMap<String, String> mSwitches = new HashMap<String, String>();
        private ArrayList<String> mArgs = new ArrayList<String>();

        // The arguments begin at index 1, since index 0 contains the executable name.
        private int mArgsBegin = 1;

        JavaCommandLine(@Nullable String[] args) {
            if (args == null || args.length == 0 || args[0] == null) {
                mArgs.add("");
            } else {
                mArgs.add(args[0]);
                appendSwitchesInternal(args, 1);
            }
            // Invariant: we always have the argv[0] program name element.
            assert mArgs.size() > 0;
        }

        @Override
        protected String[] getCommandLineArguments() {
            return mArgs.toArray(new String[mArgs.size()]);
        }

        @Override
        public boolean hasSwitch(String switchString) {
            return mSwitches.containsKey(switchString);
        }

        @Override
        public String getSwitchValue(String switchString) {
            // This is slightly round about, but needed for consistency with the NativeCommandLine
            // version which does not distinguish empty values from key not present.
            String value = mSwitches.get(switchString);
            return value == null || value.isEmpty() ? null : value;
        }

        @Override
        public Map<String, String> getSwitches() {
            return new HashMap<>(mSwitches);
        }

        @Override
        public void appendSwitch(String switchString) {
            appendSwitchWithValue(switchString, null);
        }

        /**
         * Appends a switch to the current list.
         * @param switchString the switch to add.  It should NOT start with '--' !
         * @param value the value for this switch.
         */
        @Override
        public void appendSwitchWithValue(String switchString, String value) {
            mSwitches.put(switchString, value == null ? "" : value);

            // Append the switch and update the switches/arguments divider mArgsBegin.
            String combinedSwitchString = SWITCH_PREFIX + switchString;
            if (value != null && !value.isEmpty()) {
                combinedSwitchString += SWITCH_VALUE_SEPARATOR + value;
            }

            mArgs.add(mArgsBegin++, combinedSwitchString);
        }

        @Override
        public void appendSwitchesAndArguments(String[] array) {
            appendSwitchesInternal(array, 0);
        }

        // Add the specified arguments, but skipping the first |skipCount| elements.
        private void appendSwitchesInternal(String[] array, int skipCount) {
            boolean parseSwitches = true;
            for (String arg : array) {
                if (skipCount > 0) {
                    --skipCount;
                    continue;
                }

                if (arg.equals(SWITCH_TERMINATOR)) {
                    parseSwitches = false;
                }

                if (parseSwitches && arg.startsWith(SWITCH_PREFIX)) {
                    String[] parts = arg.split(SWITCH_VALUE_SEPARATOR, 2);
                    String value = parts.length > 1 ? parts[1] : null;
                    appendSwitchWithValue(parts[0].substring(SWITCH_PREFIX.length()), value);
                } else {
                    mArgs.add(arg);
                }
            }
        }

        @Override
        public void removeSwitch(String switchString) {
            mSwitches.remove(switchString);
            String combinedSwitchString = SWITCH_PREFIX + switchString;

            // Since we permit a switch to be added multiple times, we need to remove all instances
            // from mArgs.
            for (int i = mArgsBegin - 1; i > 0; i--) {
                if (mArgs.get(i).equals(combinedSwitchString)
                        || mArgs.get(i).startsWith(combinedSwitchString + SWITCH_VALUE_SEPARATOR)) {
                    --mArgsBegin;
                    mArgs.remove(i);
                }
            }
        }
    }

    private static class NativeCommandLine extends CommandLine {
        public NativeCommandLine(@Nullable String[] args) {
            CommandLineJni.get().init(args);
        }

        @Override
        public boolean hasSwitch(String switchString) {
            return CommandLineJni.get().hasSwitch(switchString);
        }

        @Override
        public String getSwitchValue(String switchString) {
            return CommandLineJni.get().getSwitchValue(switchString);
        }

        @Override
        public Map<String, String> getSwitches() {
            HashMap<String, String> switches = new HashMap<String, String>();

            // Iterate 2 array members at a time. JNI doesn't support returning Maps, but because
            // key & value are both Strings, we can join them into a flattened String array:
            // [ key1, value1, key2, value2, ... ]
            String[] keysAndValues = CommandLineJni.get().getSwitchesFlattened();
            assert keysAndValues.length % 2 == 0 : "must have same number of keys and values";
            for (int i = 0; i < keysAndValues.length; i += 2) {
                String key = keysAndValues[i];
                String value = keysAndValues[i + 1];
                switches.put(key, value);
            }
            return switches;
        }

        @Override
        public void appendSwitch(String switchString) {
            CommandLineJni.get().appendSwitch(switchString);
        }

        @Override
        public void appendSwitchWithValue(String switchString, String value) {
            CommandLineJni.get().appendSwitchWithValue(switchString, value == null ? "" : value);
        }

        @Override
        public void appendSwitchesAndArguments(String[] array) {
            CommandLineJni.get().appendSwitchesAndArguments(array);
        }

        @Override
        public void removeSwitch(String switchString) {
            CommandLineJni.get().removeSwitch(switchString);
        }

        @Override
        public boolean isNativeImplementation() {
            return true;
        }

        @Override
        protected String[] getCommandLineArguments() {
            assert false;
            return null;
        }

        @Override
        protected void destroy() {
            // TODO(https://crbug.com/771205): Downgrade this to an assert once we have eliminated
            // tests that do this.
            throw new IllegalStateException("Can't destroy native command line after startup");
        }
    }

    @NativeMethods
    interface Natives {
        void init(String[] args);
        boolean hasSwitch(String switchString);
        String getSwitchValue(String switchString);
        String[] getSwitchesFlattened();
        void appendSwitch(String switchString);
        void appendSwitchWithValue(String switchString, String value);
        void appendSwitchesAndArguments(String[] array);
        void removeSwitch(String switchString);
    }
}
