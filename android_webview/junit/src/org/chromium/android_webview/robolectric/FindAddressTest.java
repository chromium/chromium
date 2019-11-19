// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.FindAddress;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.regex.MatchResult;

/**
 * Tests for FindAddress implementation.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FindAddressTest {
    private void assertExpectedMatch(MatchResult match, String exptectedMatch) {
        Assert.assertNotNull(match);
        Assert.assertEquals(match.group(0), exptectedMatch);
    }

    private void assertIsAddress(String address) {
        Assert.assertEquals(address, FindAddress.findAddress(address));
    }

    private boolean containsAddress(String address) {
        return FindAddress.findAddress(address) != null;
    }

    private boolean isAddress(String address) {
        return FindAddress.findAddress(address).equals(address);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFullAddress() {
        // Test US Google corporate addresses. Expects a full string match.
        assertIsAddress("1600 Amphitheatre Parkway Mountain View, CA 94043");
        assertIsAddress("201 S. Division St. Suite 500 Ann Arbor, MI 48104");
        Assert.assertTrue(containsAddress(
                "Millennium at Midtown 10 10th Street NE Suite 600 Atlanta, GA 30309"));
        assertIsAddress("9606 North MoPac Expressway Suite 400 Austin, TX 78759");
        assertIsAddress("2590 Pearl Street Suite 100 Boulder, CO 80302");
        assertIsAddress("5 Cambridge Center, Floors 3-6 Cambridge, MA 02142");
        assertIsAddress("410 Market St Suite 415 Chapel Hill, NC 27516");
        assertIsAddress("20 West Kinzie St. Chicago, IL 60654");
        assertIsAddress("114 Willits Street Birmingham, MI 48009");
        assertIsAddress("19540 Jamboree Road 2nd Floor Irvine, CA 92612");
        assertIsAddress("747 6th Street South, Kirkland, WA 98033");
        assertIsAddress("301 S. Blount St. Suite 301 Madison, WI 53703");
        assertIsAddress("76 Ninth Avenue 4th Floor New York, NY 10011");
        Assert.assertTrue(containsAddress(
                "Chelsea Markset Space, 75 Ninth Avenue 2nd and 4th Floors New York, NY 10011"));
        assertIsAddress("6425 Penn Ave. Suite 700 Pittsburgh, PA 15206");
        assertIsAddress("1818 Library Street Suite 400 Reston, VA 20190");
        assertIsAddress("345 Spear Street Floors 2-4 San Francisco, CA 94105");
        assertIsAddress("604 Arizona Avenue Santa Monica, CA 90401");
        assertIsAddress("651 N. 34th St. Seattle, WA 98103");
        Assert.assertTrue(
                isAddress("1101 New York Avenue, N.W. Second Floor Washington, DC 20005"));

        // Other tests.
        assertIsAddress("57th Street and Lake Shore Drive\nChicago, IL 60637");
        assertIsAddress("308 Congress Street Boston, MA 02210");
        Assert.assertTrue(
                containsAddress("Central Park West at 79th Street, New York, NY, 10024-5192"));
        Assert.assertTrue(containsAddress(
                "Lincoln Park | 100 34th Avenue • San Francisco, CA 94121 | 41575036"));

        Assert.assertEquals(
                FindAddress.findAddress(
                        "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do "
                        + "1600 Amphitheatre Parkway Mountain View, CA 94043 eiusmod "
                        + "tempor incididunt ut labore et dolore magna aliqua."),
                "1600 Amphitheatre Parkway Mountain View, CA 94043");

        Assert.assertEquals(FindAddress.findAddress("2590 Pearl Street Suite 100 Boulder, CO 80302 "
                                    + "6425 Penn Ave. Suite 700 Pittsburgh, PA 15206"),
                "2590 Pearl Street Suite 100 Boulder, CO 80302");

        assertIsAddress("5400 Preston Oaks Rd Dallas TX 75254");
        assertIsAddress("5400 Preston Oaks Road Dallas TX 75254");
        assertIsAddress("5400 Preston Oaks Ave Dallas TX 75254");

        Assert.assertTrue(
                containsAddress("住所は 1600 Amphitheatre Parkway Mountain View, CA 94043 です。"));

        Assert.assertFalse(containsAddress("1 st. too-short, CA 90000"));
        Assert.assertTrue(containsAddress("1 st. long enough, CA 90000"));

        Assert.assertTrue(containsAddress("1 st. some city in al 35000"));
        Assert.assertFalse(containsAddress("1 book st Aquinas et al 35000"));

        Assert.assertFalse(containsAddress("1 this comes too late: street, CA 90000"));
        Assert.assertTrue(containsAddress("1 this is ok: street, CA 90000"));

        Assert.assertFalse(
                containsAddress("1 street I love verbosity, so I'm writing an address with "
                        + "too many words CA 90000"));
        Assert.assertTrue(containsAddress("1 street 2 3 4 5 6 7 8 9 10 11 12, CA 90000"));

        assertIsAddress("79th Street 1st Floor New York City, NY 10024-5192");

        assertIsAddress("79th Street 1st Floor New York 10024-5192");
        assertIsAddress("79th Street 1st Floor New  York 10024-5192");
        Assert.assertNull(FindAddress.findAddress("79th Street 1st Floor New\nYork 10024-5192"));
        Assert.assertNull(FindAddress.findAddress("79th Street 1st Floor New, York 10024-5192"));

        Assert.assertFalse(containsAddress("123 Fake Street, Springfield, Springfield"));
        Assert.assertFalse(containsAddress("999 Street Avenue, City, ZZ 98765"));
        Assert.assertFalse(containsAddress("76 Here be dragons, CA 94043"));
        Assert.assertFalse(containsAddress("1 This, has, too* many, lines, to, be* valid"));
        Assert.assertFalse(
                containsAddress("1 Supercalifragilisticexpialidocious is too long, CA 90000"));
        Assert.assertFalse(containsAddress(""));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFullAddressWithoutZipCode() {
        assertIsAddress("1600 Amphitheatre Parkway Mountain View, CA");
        assertIsAddress("201 S. Division St. Suite 500 Ann Arbor, MI");

        // Check that addresses without a zip code are only accepted at the end of the string.
        // This isn't implied by the documentation but was the case in the old implementation
        // and fixing this bug creates a lot of false positives while fixing relatively few
        // false negatives. In these examples, "one point" is parsed as a street and "as" is a
        // state abbreviation (this is taken from a false positive reported in a bug).
        Assert.assertTrue(containsAddress("one point I was as"));
        Assert.assertTrue(containsAddress("At one point I was as ignorant as"));
        Assert.assertFalse(containsAddress("At one point I was as ignorant as them"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNumberPrefixCases() {
        Assert.assertEquals(
                FindAddress.findAddress("Cafe 21\n750 Fifth Ave. San Diego, California 92101"),
                "750 Fifth Ave. San Diego, California 92101");
        Assert.assertEquals(
                FindAddress.findAddress(
                        "Century City 15\n 10250 Santa Monica Boulevard Los Angeles, CA 90067"),
                "10250 Santa Monica Boulevard Los Angeles, CA 90067");
        Assert.assertEquals(FindAddress.findAddress("123 45\n67 My Street, Somewhere, NY 10000"),
                "67 My Street, Somewhere, NY 10000");
        assertIsAddress("123 4th Avenue, Somewhere in NY 10000");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLocationName() {
        Assert.assertFalse(FindAddress.isValidLocationName("str-eet"));
        Assert.assertFalse(FindAddress.isValidLocationName("somewhere"));

        // Test all supported street names and expected plural cases.
        Assert.assertTrue(FindAddress.isValidLocationName("alley"));
        Assert.assertTrue(FindAddress.isValidLocationName("annex"));
        Assert.assertTrue(FindAddress.isValidLocationName("arcade"));
        Assert.assertTrue(FindAddress.isValidLocationName("ave."));
        Assert.assertTrue(FindAddress.isValidLocationName("avenue"));
        Assert.assertTrue(FindAddress.isValidLocationName("alameda"));
        Assert.assertTrue(FindAddress.isValidLocationName("bayou"));
        Assert.assertTrue(FindAddress.isValidLocationName("beach"));
        Assert.assertTrue(FindAddress.isValidLocationName("bend"));
        Assert.assertTrue(FindAddress.isValidLocationName("bluff"));
        Assert.assertTrue(FindAddress.isValidLocationName("bluffs"));
        Assert.assertTrue(FindAddress.isValidLocationName("bottom"));
        Assert.assertTrue(FindAddress.isValidLocationName("boulevard"));
        Assert.assertTrue(FindAddress.isValidLocationName("branch"));
        Assert.assertTrue(FindAddress.isValidLocationName("bridge"));
        Assert.assertTrue(FindAddress.isValidLocationName("brook"));
        Assert.assertTrue(FindAddress.isValidLocationName("brooks"));
        Assert.assertTrue(FindAddress.isValidLocationName("burg"));
        Assert.assertTrue(FindAddress.isValidLocationName("burgs"));
        Assert.assertTrue(FindAddress.isValidLocationName("bypass"));
        Assert.assertTrue(FindAddress.isValidLocationName("broadway"));
        Assert.assertTrue(FindAddress.isValidLocationName("camino"));
        Assert.assertTrue(FindAddress.isValidLocationName("camp"));
        Assert.assertTrue(FindAddress.isValidLocationName("canyon"));
        Assert.assertTrue(FindAddress.isValidLocationName("cape"));
        Assert.assertTrue(FindAddress.isValidLocationName("causeway"));
        Assert.assertTrue(FindAddress.isValidLocationName("center"));
        Assert.assertTrue(FindAddress.isValidLocationName("centers"));
        Assert.assertTrue(FindAddress.isValidLocationName("circle"));
        Assert.assertTrue(FindAddress.isValidLocationName("circles"));
        Assert.assertTrue(FindAddress.isValidLocationName("cliff"));
        Assert.assertTrue(FindAddress.isValidLocationName("cliffs"));
        Assert.assertTrue(FindAddress.isValidLocationName("club"));
        Assert.assertTrue(FindAddress.isValidLocationName("common"));
        Assert.assertTrue(FindAddress.isValidLocationName("corner"));
        Assert.assertTrue(FindAddress.isValidLocationName("corners"));
        Assert.assertTrue(FindAddress.isValidLocationName("course"));
        Assert.assertTrue(FindAddress.isValidLocationName("court"));
        Assert.assertTrue(FindAddress.isValidLocationName("courts"));
        Assert.assertTrue(FindAddress.isValidLocationName("cove"));
        Assert.assertTrue(FindAddress.isValidLocationName("coves"));
        Assert.assertTrue(FindAddress.isValidLocationName("creek"));
        Assert.assertTrue(FindAddress.isValidLocationName("crescent"));
        Assert.assertTrue(FindAddress.isValidLocationName("crest"));
        Assert.assertTrue(FindAddress.isValidLocationName("crossing"));
        Assert.assertTrue(FindAddress.isValidLocationName("crossroad"));
        Assert.assertTrue(FindAddress.isValidLocationName("curve"));
        Assert.assertTrue(FindAddress.isValidLocationName("circulo"));
        Assert.assertTrue(FindAddress.isValidLocationName("dale"));
        Assert.assertTrue(FindAddress.isValidLocationName("dam"));
        Assert.assertTrue(FindAddress.isValidLocationName("divide"));
        Assert.assertTrue(FindAddress.isValidLocationName("drive"));
        Assert.assertTrue(FindAddress.isValidLocationName("drives"));
        Assert.assertTrue(FindAddress.isValidLocationName("estate"));
        Assert.assertTrue(FindAddress.isValidLocationName("estates"));
        Assert.assertTrue(FindAddress.isValidLocationName("expressway"));
        Assert.assertTrue(FindAddress.isValidLocationName("extension"));
        Assert.assertTrue(FindAddress.isValidLocationName("extensions"));
        Assert.assertTrue(FindAddress.isValidLocationName("fall"));
        Assert.assertTrue(FindAddress.isValidLocationName("falls"));
        Assert.assertTrue(FindAddress.isValidLocationName("ferry"));
        Assert.assertTrue(FindAddress.isValidLocationName("field"));
        Assert.assertTrue(FindAddress.isValidLocationName("fields"));
        Assert.assertTrue(FindAddress.isValidLocationName("flat"));
        Assert.assertTrue(FindAddress.isValidLocationName("flats"));
        Assert.assertTrue(FindAddress.isValidLocationName("ford"));
        Assert.assertTrue(FindAddress.isValidLocationName("fords"));
        Assert.assertTrue(FindAddress.isValidLocationName("forest"));
        Assert.assertTrue(FindAddress.isValidLocationName("forge"));
        Assert.assertTrue(FindAddress.isValidLocationName("forges"));
        Assert.assertTrue(FindAddress.isValidLocationName("fork"));
        Assert.assertTrue(FindAddress.isValidLocationName("forks"));
        Assert.assertTrue(FindAddress.isValidLocationName("fort"));
        Assert.assertTrue(FindAddress.isValidLocationName("freeway"));
        Assert.assertTrue(FindAddress.isValidLocationName("garden"));
        Assert.assertTrue(FindAddress.isValidLocationName("gardens"));
        Assert.assertTrue(FindAddress.isValidLocationName("gateway"));
        Assert.assertTrue(FindAddress.isValidLocationName("glen"));
        Assert.assertTrue(FindAddress.isValidLocationName("glens"));
        Assert.assertTrue(FindAddress.isValidLocationName("green"));
        Assert.assertTrue(FindAddress.isValidLocationName("greens"));
        Assert.assertTrue(FindAddress.isValidLocationName("grove"));
        Assert.assertTrue(FindAddress.isValidLocationName("groves"));
        Assert.assertTrue(FindAddress.isValidLocationName("harbor"));
        Assert.assertTrue(FindAddress.isValidLocationName("harbors"));
        Assert.assertTrue(FindAddress.isValidLocationName("haven"));
        Assert.assertTrue(FindAddress.isValidLocationName("heights"));
        Assert.assertTrue(FindAddress.isValidLocationName("highway"));
        Assert.assertTrue(FindAddress.isValidLocationName("hill"));
        Assert.assertTrue(FindAddress.isValidLocationName("hills"));
        Assert.assertTrue(FindAddress.isValidLocationName("hollow"));
        Assert.assertTrue(FindAddress.isValidLocationName("inlet"));
        Assert.assertTrue(FindAddress.isValidLocationName("island"));
        Assert.assertTrue(FindAddress.isValidLocationName("islands"));
        Assert.assertTrue(FindAddress.isValidLocationName("isle"));
        Assert.assertTrue(FindAddress.isValidLocationName("junction"));
        Assert.assertTrue(FindAddress.isValidLocationName("junctions"));
        Assert.assertTrue(FindAddress.isValidLocationName("key"));
        Assert.assertTrue(FindAddress.isValidLocationName("keys"));
        Assert.assertTrue(FindAddress.isValidLocationName("knoll"));
        Assert.assertTrue(FindAddress.isValidLocationName("knolls"));
        Assert.assertTrue(FindAddress.isValidLocationName("lake"));
        Assert.assertTrue(FindAddress.isValidLocationName("lakes"));
        Assert.assertTrue(FindAddress.isValidLocationName("land"));
        Assert.assertTrue(FindAddress.isValidLocationName("landing"));
        Assert.assertTrue(FindAddress.isValidLocationName("lane"));
        Assert.assertTrue(FindAddress.isValidLocationName("light"));
        Assert.assertTrue(FindAddress.isValidLocationName("lights"));
        Assert.assertTrue(FindAddress.isValidLocationName("loaf"));
        Assert.assertTrue(FindAddress.isValidLocationName("lock"));
        Assert.assertTrue(FindAddress.isValidLocationName("locks"));
        Assert.assertTrue(FindAddress.isValidLocationName("lodge"));
        Assert.assertTrue(FindAddress.isValidLocationName("loop"));
        Assert.assertTrue(FindAddress.isValidLocationName("mall"));
        Assert.assertTrue(FindAddress.isValidLocationName("manor"));
        Assert.assertTrue(FindAddress.isValidLocationName("manors"));
        Assert.assertTrue(FindAddress.isValidLocationName("meadow"));
        Assert.assertTrue(FindAddress.isValidLocationName("meadows"));
        Assert.assertTrue(FindAddress.isValidLocationName("mews"));
        Assert.assertTrue(FindAddress.isValidLocationName("mill"));
        Assert.assertTrue(FindAddress.isValidLocationName("mills"));
        Assert.assertTrue(FindAddress.isValidLocationName("mission"));
        Assert.assertTrue(FindAddress.isValidLocationName("motorway"));
        Assert.assertTrue(FindAddress.isValidLocationName("mount"));
        Assert.assertTrue(FindAddress.isValidLocationName("mountain"));
        Assert.assertTrue(FindAddress.isValidLocationName("mountains"));
        Assert.assertTrue(FindAddress.isValidLocationName("neck"));
        Assert.assertTrue(FindAddress.isValidLocationName("orchard"));
        Assert.assertTrue(FindAddress.isValidLocationName("oval"));
        Assert.assertTrue(FindAddress.isValidLocationName("overpass"));
        Assert.assertTrue(FindAddress.isValidLocationName("park"));
        Assert.assertTrue(FindAddress.isValidLocationName("parks"));
        Assert.assertTrue(FindAddress.isValidLocationName("parkway"));
        Assert.assertTrue(FindAddress.isValidLocationName("parkways"));
        Assert.assertTrue(FindAddress.isValidLocationName("pass"));
        Assert.assertTrue(FindAddress.isValidLocationName("passage"));
        Assert.assertTrue(FindAddress.isValidLocationName("path"));
        Assert.assertTrue(FindAddress.isValidLocationName("pike"));
        Assert.assertTrue(FindAddress.isValidLocationName("pine"));
        Assert.assertTrue(FindAddress.isValidLocationName("pines"));
        Assert.assertTrue(FindAddress.isValidLocationName("plain"));
        Assert.assertTrue(FindAddress.isValidLocationName("plains"));
        Assert.assertTrue(FindAddress.isValidLocationName("plaza"));
        Assert.assertTrue(FindAddress.isValidLocationName("point"));
        Assert.assertTrue(FindAddress.isValidLocationName("points"));
        Assert.assertTrue(FindAddress.isValidLocationName("port"));
        Assert.assertTrue(FindAddress.isValidLocationName("ports"));
        Assert.assertTrue(FindAddress.isValidLocationName("prairie"));
        Assert.assertTrue(FindAddress.isValidLocationName("privada"));
        Assert.assertTrue(FindAddress.isValidLocationName("radial"));
        Assert.assertTrue(FindAddress.isValidLocationName("ramp"));
        Assert.assertTrue(FindAddress.isValidLocationName("ranch"));
        Assert.assertTrue(FindAddress.isValidLocationName("rapid"));
        Assert.assertTrue(FindAddress.isValidLocationName("rapids"));
        Assert.assertTrue(FindAddress.isValidLocationName("rd"));
        Assert.assertTrue(FindAddress.isValidLocationName("rd."));
        Assert.assertTrue(FindAddress.isValidLocationName("rest"));
        Assert.assertTrue(FindAddress.isValidLocationName("ridge"));
        Assert.assertTrue(FindAddress.isValidLocationName("ridges"));
        Assert.assertTrue(FindAddress.isValidLocationName("river"));
        Assert.assertTrue(FindAddress.isValidLocationName("road"));
        Assert.assertTrue(FindAddress.isValidLocationName("roads"));
        Assert.assertTrue(FindAddress.isValidLocationName("route"));
        Assert.assertTrue(FindAddress.isValidLocationName("row"));
        Assert.assertTrue(FindAddress.isValidLocationName("rue"));
        Assert.assertTrue(FindAddress.isValidLocationName("run"));
        Assert.assertTrue(FindAddress.isValidLocationName("shoal"));
        Assert.assertTrue(FindAddress.isValidLocationName("shoals"));
        Assert.assertTrue(FindAddress.isValidLocationName("shore"));
        Assert.assertTrue(FindAddress.isValidLocationName("shores"));
        Assert.assertTrue(FindAddress.isValidLocationName("skyway"));
        Assert.assertTrue(FindAddress.isValidLocationName("spring"));
        Assert.assertTrue(FindAddress.isValidLocationName("springs"));
        Assert.assertTrue(FindAddress.isValidLocationName("spur"));
        Assert.assertTrue(FindAddress.isValidLocationName("spurs"));
        Assert.assertTrue(FindAddress.isValidLocationName("square"));
        Assert.assertTrue(FindAddress.isValidLocationName("squares"));
        Assert.assertTrue(FindAddress.isValidLocationName("station"));
        Assert.assertTrue(FindAddress.isValidLocationName("stravenue"));
        Assert.assertTrue(FindAddress.isValidLocationName("stream"));
        Assert.assertTrue(FindAddress.isValidLocationName("st."));
        Assert.assertTrue(FindAddress.isValidLocationName("street"));
        Assert.assertTrue(FindAddress.isValidLocationName("streets"));
        Assert.assertTrue(FindAddress.isValidLocationName("summit"));
        Assert.assertTrue(FindAddress.isValidLocationName("speedway"));
        Assert.assertTrue(FindAddress.isValidLocationName("terrace"));
        Assert.assertTrue(FindAddress.isValidLocationName("throughway"));
        Assert.assertTrue(FindAddress.isValidLocationName("trace"));
        Assert.assertTrue(FindAddress.isValidLocationName("track"));
        Assert.assertTrue(FindAddress.isValidLocationName("trafficway"));
        Assert.assertTrue(FindAddress.isValidLocationName("trail"));
        Assert.assertTrue(FindAddress.isValidLocationName("tunnel"));
        Assert.assertTrue(FindAddress.isValidLocationName("turnpike"));
        Assert.assertTrue(FindAddress.isValidLocationName("underpass"));
        Assert.assertTrue(FindAddress.isValidLocationName("union"));
        Assert.assertTrue(FindAddress.isValidLocationName("unions"));
        Assert.assertTrue(FindAddress.isValidLocationName("valley"));
        Assert.assertTrue(FindAddress.isValidLocationName("valleys"));
        Assert.assertTrue(FindAddress.isValidLocationName("viaduct"));
        Assert.assertTrue(FindAddress.isValidLocationName("view"));
        Assert.assertTrue(FindAddress.isValidLocationName("views"));
        Assert.assertTrue(FindAddress.isValidLocationName("village"));
        Assert.assertTrue(FindAddress.isValidLocationName("villages"));
        Assert.assertTrue(FindAddress.isValidLocationName("ville"));
        Assert.assertTrue(FindAddress.isValidLocationName("vista"));
        Assert.assertTrue(FindAddress.isValidLocationName("walk"));
        Assert.assertTrue(FindAddress.isValidLocationName("walks"));
        Assert.assertTrue(FindAddress.isValidLocationName("wall"));
        Assert.assertTrue(FindAddress.isValidLocationName("way"));
        Assert.assertTrue(FindAddress.isValidLocationName("ways"));
        Assert.assertTrue(FindAddress.isValidLocationName("well"));
        Assert.assertTrue(FindAddress.isValidLocationName("wells"));
        Assert.assertTrue(FindAddress.isValidLocationName("xing"));
        Assert.assertTrue(FindAddress.isValidLocationName("xrd"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZipCode() {
        Assert.assertTrue(FindAddress.isValidZipCode("90000"));
        Assert.assertTrue(FindAddress.isValidZipCode("01234"));
        Assert.assertTrue(FindAddress.isValidZipCode("99999-9999"));

        Assert.assertFalse(FindAddress.isValidZipCode("999999999"));
        Assert.assertFalse(FindAddress.isValidZipCode("9999-99999"));
        Assert.assertFalse(FindAddress.isValidZipCode("999999999-"));
        Assert.assertFalse(FindAddress.isValidZipCode("99999-999a"));
        Assert.assertFalse(FindAddress.isValidZipCode("99999--9999"));
        Assert.assertFalse(FindAddress.isValidZipCode("90000o"));
        Assert.assertFalse(FindAddress.isValidZipCode("90000-"));

        // Test the state index against the zip range table.
        Assert.assertTrue(FindAddress.isValidZipCode("99000", "AK"));
        Assert.assertTrue(FindAddress.isValidZipCode("99000", "Alaska"));
        Assert.assertTrue(FindAddress.isValidZipCode("35000", "AL"));
        Assert.assertTrue(FindAddress.isValidZipCode("36000", "Alabama"));
        Assert.assertTrue(FindAddress.isValidZipCode("71000", "AR"));
        Assert.assertTrue(FindAddress.isValidZipCode("72000", "Arkansas"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "AS"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "American Samoa"));
        Assert.assertTrue(FindAddress.isValidZipCode("85000", "AZ"));
        Assert.assertTrue(FindAddress.isValidZipCode("86000", "Arizona"));
        Assert.assertTrue(FindAddress.isValidZipCode("90000", "CA"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "California"));
        Assert.assertTrue(FindAddress.isValidZipCode("80000", "CO"));
        Assert.assertTrue(FindAddress.isValidZipCode("81000", "Colorado"));
        Assert.assertTrue(FindAddress.isValidZipCode("06000", "CT"));
        Assert.assertTrue(FindAddress.isValidZipCode("06000", "Connecticut"));
        Assert.assertTrue(FindAddress.isValidZipCode("20000", "DC"));
        Assert.assertTrue(FindAddress.isValidZipCode("20000", "District of Columbia"));
        Assert.assertTrue(FindAddress.isValidZipCode("19000", "DE"));
        Assert.assertTrue(FindAddress.isValidZipCode("19000", "Delaware"));
        Assert.assertTrue(FindAddress.isValidZipCode("32000", "FL"));
        Assert.assertTrue(FindAddress.isValidZipCode("34000", "Florida"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "FM"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "Federated States of Micronesia"));
        Assert.assertTrue(FindAddress.isValidZipCode("30000", "GA"));
        Assert.assertTrue(FindAddress.isValidZipCode("31000", "Georgia"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "GU"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "Guam"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "HI"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "Hawaii"));
        Assert.assertTrue(FindAddress.isValidZipCode("50000", "IA"));
        Assert.assertTrue(FindAddress.isValidZipCode("52000", "Iowa"));
        Assert.assertTrue(FindAddress.isValidZipCode("83000", "ID"));
        Assert.assertTrue(FindAddress.isValidZipCode("83000", "Idaho"));
        Assert.assertTrue(FindAddress.isValidZipCode("60000", "IL"));
        Assert.assertTrue(FindAddress.isValidZipCode("62000", "Illinois"));
        Assert.assertTrue(FindAddress.isValidZipCode("46000", "IN"));
        Assert.assertTrue(FindAddress.isValidZipCode("47000", "Indiana"));
        Assert.assertTrue(FindAddress.isValidZipCode("66000", "KS"));
        Assert.assertTrue(FindAddress.isValidZipCode("67000", "Kansas"));
        Assert.assertTrue(FindAddress.isValidZipCode("40000", "KY"));
        Assert.assertTrue(FindAddress.isValidZipCode("42000", "Kentucky"));
        Assert.assertTrue(FindAddress.isValidZipCode("70000", "LA"));
        Assert.assertTrue(FindAddress.isValidZipCode("71000", "Louisiana"));
        Assert.assertTrue(FindAddress.isValidZipCode("01000", "MA"));
        Assert.assertTrue(FindAddress.isValidZipCode("02000", "Massachusetts"));
        Assert.assertTrue(FindAddress.isValidZipCode("20000", "MD"));
        Assert.assertTrue(FindAddress.isValidZipCode("21000", "Maryland"));
        Assert.assertTrue(FindAddress.isValidZipCode("03000", "ME"));
        Assert.assertTrue(FindAddress.isValidZipCode("04000", "Maine"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "MH"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "Marshall Islands"));
        Assert.assertTrue(FindAddress.isValidZipCode("48000", "MI"));
        Assert.assertTrue(FindAddress.isValidZipCode("49000", "Michigan"));
        Assert.assertTrue(FindAddress.isValidZipCode("55000", "MN"));
        Assert.assertTrue(FindAddress.isValidZipCode("56000", "Minnesota"));
        Assert.assertTrue(FindAddress.isValidZipCode("63000", "MO"));
        Assert.assertTrue(FindAddress.isValidZipCode("65000", "Missouri"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "MP"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "Northern Mariana Islands"));
        Assert.assertTrue(FindAddress.isValidZipCode("38000", "MS"));
        Assert.assertTrue(FindAddress.isValidZipCode("39000", "Mississippi"));
        Assert.assertTrue(FindAddress.isValidZipCode("55000", "MT"));
        Assert.assertTrue(FindAddress.isValidZipCode("56000", "Montana"));
        Assert.assertTrue(FindAddress.isValidZipCode("27000", "NC"));
        Assert.assertTrue(FindAddress.isValidZipCode("28000", "North Carolina"));
        Assert.assertTrue(FindAddress.isValidZipCode("58000", "ND"));
        Assert.assertTrue(FindAddress.isValidZipCode("58000", "North Dakota"));
        Assert.assertTrue(FindAddress.isValidZipCode("68000", "NE"));
        Assert.assertTrue(FindAddress.isValidZipCode("69000", "Nebraska"));
        Assert.assertTrue(FindAddress.isValidZipCode("03000", "NH"));
        Assert.assertTrue(FindAddress.isValidZipCode("04000", "New Hampshire"));
        Assert.assertTrue(FindAddress.isValidZipCode("07000", "NJ"));
        Assert.assertTrue(FindAddress.isValidZipCode("08000", "New Jersey"));
        Assert.assertTrue(FindAddress.isValidZipCode("87000", "NM"));
        Assert.assertTrue(FindAddress.isValidZipCode("88000", "New Mexico"));
        Assert.assertTrue(FindAddress.isValidZipCode("88000", "NV"));
        Assert.assertTrue(FindAddress.isValidZipCode("89000", "Nevada"));
        Assert.assertTrue(FindAddress.isValidZipCode("10000", "NY"));
        Assert.assertTrue(FindAddress.isValidZipCode("14000", "New York"));
        Assert.assertTrue(FindAddress.isValidZipCode("43000", "OH"));
        Assert.assertTrue(FindAddress.isValidZipCode("45000", "Ohio"));
        Assert.assertTrue(FindAddress.isValidZipCode("73000", "OK"));
        Assert.assertTrue(FindAddress.isValidZipCode("74000", "Oklahoma"));
        Assert.assertTrue(FindAddress.isValidZipCode("97000", "OR"));
        Assert.assertTrue(FindAddress.isValidZipCode("97000", "Oregon"));
        Assert.assertTrue(FindAddress.isValidZipCode("15000", "PA"));
        Assert.assertTrue(FindAddress.isValidZipCode("19000", "Pennsylvania"));
        Assert.assertTrue(FindAddress.isValidZipCode("06000", "PR"));
        Assert.assertTrue(FindAddress.isValidZipCode("06000", "Puerto Rico"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "PW"));
        Assert.assertTrue(FindAddress.isValidZipCode("96000", "Palau"));
        Assert.assertTrue(FindAddress.isValidZipCode("02000", "RI"));
        Assert.assertTrue(FindAddress.isValidZipCode("02000", "Rhode Island"));
        Assert.assertTrue(FindAddress.isValidZipCode("29000", "SC"));
        Assert.assertTrue(FindAddress.isValidZipCode("29000", "South Carolina"));
        Assert.assertTrue(FindAddress.isValidZipCode("57000", "SD"));
        Assert.assertTrue(FindAddress.isValidZipCode("57000", "South Dakota"));
        Assert.assertTrue(FindAddress.isValidZipCode("37000", "TN"));
        Assert.assertTrue(FindAddress.isValidZipCode("38000", "Tennessee"));
        Assert.assertTrue(FindAddress.isValidZipCode("75000", "TX"));
        Assert.assertTrue(FindAddress.isValidZipCode("79000", "Texas"));
        Assert.assertTrue(FindAddress.isValidZipCode("84000", "UT"));
        Assert.assertTrue(FindAddress.isValidZipCode("84000", "Utah"));
        Assert.assertTrue(FindAddress.isValidZipCode("22000", "VA"));
        Assert.assertTrue(FindAddress.isValidZipCode("24000", "Virginia"));
        Assert.assertTrue(FindAddress.isValidZipCode("06000", "VI"));
        Assert.assertTrue(FindAddress.isValidZipCode("09000", "Virgin Islands"));
        Assert.assertTrue(FindAddress.isValidZipCode("05000", "VT"));
        Assert.assertTrue(FindAddress.isValidZipCode("05000", "Vermont"));
        Assert.assertTrue(FindAddress.isValidZipCode("98000", "WA"));
        Assert.assertTrue(FindAddress.isValidZipCode("99000", "Washington"));
        Assert.assertTrue(FindAddress.isValidZipCode("53000", "WI"));
        Assert.assertTrue(FindAddress.isValidZipCode("54000", "Wisconsin"));
        Assert.assertTrue(FindAddress.isValidZipCode("24000", "WV"));
        Assert.assertTrue(FindAddress.isValidZipCode("26000", "West Virginia"));
        Assert.assertTrue(FindAddress.isValidZipCode("82000", "WY"));
        Assert.assertTrue(FindAddress.isValidZipCode("83000", "Wyoming"));
    }
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMatchState() {
        // The complete set of state codes and names is tested together
        // with their returned state indices in the zip code test.
        assertExpectedMatch(FindAddress.matchState("CALIFORNIA", 0), "CALIFORNIA");
        assertExpectedMatch(FindAddress.matchState("ca", 0), "ca");

        assertExpectedMatch(FindAddress.matchState(" CALIFORNIA", 1), "CALIFORNIA");
        assertExpectedMatch(FindAddress.matchState(" ca", 1), "ca");

        Assert.assertNull(FindAddress.matchState("notcalifornia", 3));
        Assert.assertNull(FindAddress.matchState("californi", 0));
        Assert.assertNull(FindAddress.matchState("northern mariana", 0));
        Assert.assertNull(FindAddress.matchState("northern mariana island", 0));
        Assert.assertNull(FindAddress.matchState("zz", 0));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetHouseNumber() {
        assertExpectedMatch(FindAddress.matchHouseNumber("4", 0), "4");

        // Matches not at the start of the string should be preceded by a valid delimiter.
        assertExpectedMatch(FindAddress.matchHouseNumber(" 4", 1), "4");
        assertExpectedMatch(FindAddress.matchHouseNumber(",4", 1), "4");
        Assert.assertNull(FindAddress.matchHouseNumber("x4", 1));

        // Matches should be followed by a valid delimiter.
        assertExpectedMatch(FindAddress.matchHouseNumber("4,5", 0), "4");
        Assert.assertNull(FindAddress.matchHouseNumber("4?", 0));
        Assert.assertNull(FindAddress.matchHouseNumber("4:", 0));

        // Quotes are valid delimiters.
        assertExpectedMatch(FindAddress.matchHouseNumber("\"4\"", 1), "4");
        assertExpectedMatch(FindAddress.matchHouseNumber("'4'", 1), "4");

        // Matches shouldn't include the delimiter, or anything after it.
        assertExpectedMatch(FindAddress.matchHouseNumber("4 my house", 0), "4");

        // One is a valid house number.
        assertExpectedMatch(FindAddress.matchHouseNumber("one", 0), "one");

        // One can't be an ordinal though.
        Assert.assertNull(FindAddress.matchHouseNumber("oneth", 0));

        // House numbers can be followed by a single letter.
        assertExpectedMatch(FindAddress.matchHouseNumber("1A", 0), "1A");

        // But not two.
        Assert.assertNull(FindAddress.matchHouseNumber("1AA", 0));

        // Except if it's a valid ordinal.
        assertExpectedMatch(FindAddress.matchHouseNumber("1st", 0), "1st");
        assertExpectedMatch(FindAddress.matchHouseNumber("1ST", 0), "1ST");
        assertExpectedMatch(FindAddress.matchHouseNumber("11th", 0), "11th");
        assertExpectedMatch(FindAddress.matchHouseNumber("21st", 0), "21st");

        assertExpectedMatch(FindAddress.matchHouseNumber("2nd", 0), "2nd");
        assertExpectedMatch(FindAddress.matchHouseNumber("12th", 0), "12th");
        assertExpectedMatch(FindAddress.matchHouseNumber("22nd", 0), "22nd");

        assertExpectedMatch(FindAddress.matchHouseNumber("3rd", 0), "3rd");
        assertExpectedMatch(FindAddress.matchHouseNumber("13th", 0), "13th");
        assertExpectedMatch(FindAddress.matchHouseNumber("23rd", 0), "23rd");

        Assert.assertNull(FindAddress.matchHouseNumber("11st", 0));
        Assert.assertNull(FindAddress.matchHouseNumber("21th", 0));
        Assert.assertNull(FindAddress.matchHouseNumber("1nd", 0));

        // These two cases are different from the original C++
        // implementation (which didn't match numbers in these cases).
        assertExpectedMatch(FindAddress.matchHouseNumber("111th", 0), "111th");
        assertExpectedMatch(FindAddress.matchHouseNumber("011th", 0), "011th");

        // This case used to match, but now doesn't.
        Assert.assertNull(FindAddress.matchHouseNumber("211st", 0));

        // Hypenated numbers are OK.
        assertExpectedMatch(FindAddress.matchHouseNumber("1-201", 0), "1-201");
        assertExpectedMatch(FindAddress.matchHouseNumber("1-one", 0), "1-one");

        // But a trailing hypen isn't valid.
        Assert.assertNull(FindAddress.matchHouseNumber("1- ", 0));
        Assert.assertNull(FindAddress.matchHouseNumber("1-word", 0));

        // Ordinals can be part of a hyphenated number.
        assertExpectedMatch(FindAddress.matchHouseNumber("1-1st", 0), "1-1st");

        // Limit of 5 digits at most.
        assertExpectedMatch(FindAddress.matchHouseNumber("12345", 0), "12345");
        Assert.assertNull(FindAddress.matchHouseNumber("123456", 0));

        // Limit applies to the whole match, not the components.
        Assert.assertNull(FindAddress.matchHouseNumber("123-456", 0));
    }
}
