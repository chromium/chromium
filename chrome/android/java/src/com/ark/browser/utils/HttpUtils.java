package com.ark.browser.utils;

import androidx.annotation.NonNull;

import com.ark.browser.core.UserAgentManager;

import org.chromium.base.Log;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.IllegalCharsetNameException;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.GZIPInputStream;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;

public class HttpUtils {


    public static void get(String url, Callback callback) {
        ThreadPool.execute(() -> {

            HttpURLConnection connection = null;
            try {
                connection = (HttpURLConnection) new URL(url).openConnection();
                connection.setConnectTimeout(10000);
                connection.setReadTimeout(10000);
                connection.addRequestProperty("User-Agent", UserAgentManager.getDefaultUserAgent().getString());

                connection.setRequestMethod("GET");

                connection.connect();

                int code = connection.getResponseCode();


                Map<String, List<String>> map = connection.getHeaderFields();

                for (Map.Entry<String, List<String>> entry : map.entrySet()) {
                    Log.d("HttpUtils", "key=" + entry.getKey() + " value=" + entry.getValue());
                }

                String contentType = connection.getHeaderField("Content-Type");
                Charset charset = Charset.forName(getCharsetFromContentType(contentType));
                if (code == HttpURLConnection.HTTP_OK) {
                    InputStream bodyStream = connection.getInputStream();


                    String contentEncoding = connection.getHeaderField("Content-Encoding");
                    Log.d("HttpUtils", "contentEncoding=" + contentEncoding);
                    if ("gzip".equalsIgnoreCase(contentEncoding)) {
                        bodyStream = new GZIPInputStream(bodyStream);
                    } else if ("deflate".equalsIgnoreCase(contentEncoding)) {
                        bodyStream = new InflaterInputStream(bodyStream, new Inflater(true));
                    }

                    String body = isToStream(bodyStream, charset);

//                    ByteBuffer buffer = readToByteBuffer(bodyStream);
//                    String body = charset.decode(buffer).toString();
                    Log.d("HttpUtils", "body=" + body);

                    ThreadPool.runOnUIThread(() -> {
                        try {
                            callback.onSuccess(body);
                        } catch (Exception e) {
                            callback.onFailed(e);
                        }
                    });
                    bodyStream.close();
                } else {
                    throw new Exception(isToStream(connection.getErrorStream(), charset));
                }
            } catch (Exception e) {
                ThreadPool.runOnUIThread(() -> callback.onFailed(e));
            } finally {
                if (connection != null) {
                    connection.disconnect();
                }
            }

        });
    }

    private static String isToStream(InputStream is, @NonNull Charset charset) throws Exception {
        String buf;
        try (InputStreamReader inputStreamReader = new InputStreamReader(is, charset);
             BufferedReader reader = new BufferedReader(inputStreamReader)
        ) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            is.close();
            buf = sb.toString();
            return buf;
        }
    }

    public static ByteBuffer readToByteBuffer(InputStream is) throws IOException {
        final int bufferSize = 32 * 1024;
        final byte[] readBuffer = new byte[bufferSize];
        try (ByteArrayOutputStream outStream = new ByteArrayOutputStream(bufferSize)) {
            int read;
            while ((read = is.read(readBuffer)) != -1) {
                outStream.write(readBuffer, 0, read);
            }
            return ByteBuffer.wrap(outStream.toByteArray());
        }
    }

    private static final Pattern charsetPattern = Pattern.compile("(?i)\\bcharset=\\s*(?:[\"'])?([^\\s,;\"']*)");
    public static final String defaultCharset = "UTF-8";

    public static String getCharsetFromContentType(String contentType) {
        if (contentType == null) return null;
        Matcher m = charsetPattern.matcher(contentType);
        if (m.find()) {
            String charset = m.group(1).trim();
            charset = charset.replace("charset=", "");
            return validateCharset(charset);
        }
        return defaultCharset;
    }

    private static String validateCharset(String cs) {
        if (cs == null || cs.length() == 0) return null;
        cs = cs.trim().replaceAll("[\"']", "");
        try {
            if (Charset.isSupported(cs)) return cs;
            cs = cs.toUpperCase(Locale.ENGLISH);
            if (Charset.isSupported(cs)) return cs;
        } catch (IllegalCharsetNameException e) {
            // if our this charset matching fails.... we just take the default
        }
        return null;
    }


    public interface Callback {
        void onFailed(Exception e);
        void onSuccess(String body) throws Exception;
    }

}

